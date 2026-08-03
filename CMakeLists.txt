cmake_minimum_required(VERSION 3.20)
project(NereusSDR VERSION 0.5.2 LANGUAGES C CXX)

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
if(APPLE)
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
option(NEREUSSDR_AGGRESSIVE_OPT "Use -O3 (instead of -O2) for release builds" ON)
# 2026-05-26 KG4VCF: LTO default flipped to OFF.  With LTO enabled on
# the shared NereusCore/NereusGui object libraries, every test binary
# (589 of them) inherits the LTO bitcode and triggers a multi-minute LTO
# link of its own.  Net effect on a clean build: 1+ hour, load avg 250+,
# unusable while building.  Per-target IPO=FALSE on tests doesn't
# undo the bitcode embed.  Default LTO off makes dev builds fast;
# release artifacts can opt in via `cmake -DNEREUSSDR_ENABLE_LTO=ON`.
option(NEREUSSDR_ENABLE_LTO     "Enable link-time optimization (slow build; release artifacts only)" OFF)

if(NEREUSSDR_AGGRESSIVE_OPT)
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
        message(STATUS "NereusSDR: -O3 enabled for ${CMAKE_BUILD_TYPE}")
    endif()
endif()

if(NEREUSSDR_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _lto_ok OUTPUT _lto_err)
    if(_lto_ok)
        # 2026-05-26 KG4VCF: LTO is applied PER-TARGET below (only the
        # main NereusSDR executable + the parallel NereusCore_LTO/
        # NereusGui_LTO object libraries it links).  We deliberately do
        # NOT set the global CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO
        # flag, because that would make the shared NereusCore/NereusGui
        # (used by 589 test executables) produce LTO bitcode, and every
        # test link would then trigger a multi-minute LTO pass.  Net
        # effect of the old global setting on a clean build: ~1 hour,
        # load avg 250+, dev-loop unusable while building.
        set(NEREUSSDR_LTO_AVAILABLE TRUE)
        message(STATUS "NereusSDR: link-time optimization enabled (main app only)")
    else()
        message(WARNING "NereusSDR: LTO requested but unsupported -- ${_lto_err}")
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
)

# WebSockets is REQUIRED (not optional) because both the TCI server and the
# FreeDV Reporter / PSK Reporter spot ingest depend on it. v0.5.0 shipped a
# macOS x86_64 build with `qtwebsockets` missing from the install-qt-action
# modules list; CMake's QUIET find_package silently left HAVE_WEBSOCKETS
# undefined and the resulting binary disabled both features at runtime with
# only a warning in the log. Mirroring the v0.1.1 NEREUS_GPU_SPECTRUM lesson
# (see release.yml Configure step): if a feature is mandatory, the build
# system has to refuse to configure without it.

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
# Detection-only here; actual PipeWire sources land behind NEREUS_HAVE_PIPEWIRE
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
# explicitly requested -DNEREUS_GPU_SPECTRUM=ON. Users saw a blank
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
#      genuinely want the CPU path must pass -DNEREUS_GPU_SPECTRUM=OFF
#      explicitly to acknowledge it.
# --------------------------------------------------------------------------
option(NEREUS_GPU_SPECTRUM "Enable GPU-accelerated spectrum/waterfall via QRhi (requires Qt 6.7+ with ShaderTools and Gui private headers)" ON)

# Remember whether the caller requested it before any auto-disable, so
# we can produce a hard failure if we can't honor the request.
set(_nereus_gpu_spectrum_requested ${NEREUS_GPU_SPECTRUM})
set(_nereus_gpu_fail_reason "")

if(NEREUS_GPU_SPECTRUM)
    if(Qt6_VERSION VERSION_LESS "6.7")
        set(_nereus_gpu_fail_reason
            "Qt ${Qt6_VERSION} is older than 6.7 (QRhiWidget requires 6.7+)")
        set(NEREUS_GPU_SPECTRUM OFF)
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
        set(_nereus_have_shadertools FALSE)
        set(_nereus_have_guiprivate  FALSE)
        if(Qt6ShaderTools_FOUND OR TARGET Qt6::ShaderTools)
            set(_nereus_have_shadertools TRUE)
        endif()
        if(Qt6GuiPrivate_FOUND OR TARGET Qt6::GuiPrivate)
            set(_nereus_have_guiprivate TRUE)
        endif()

        if(_nereus_have_shadertools AND _nereus_have_guiprivate)
            message(STATUS "QRhi support enabled — GPU spectrum/waterfall active")
        else()
            set(_nereus_gpu_fail_reason "Qt6 private modules missing:")
            if(NOT _nereus_have_shadertools)
                string(APPEND _nereus_gpu_fail_reason " ShaderTools")
            endif()
            if(NOT _nereus_have_guiprivate)
                string(APPEND _nereus_gpu_fail_reason " GuiPrivate")
            endif()
            set(NEREUS_GPU_SPECTRUM OFF)
        endif()
    endif()
endif()

# Fail loudly if the caller asked for GPU spectrum and we couldn't
# deliver it. This prevents a repeat of the v0.1.1 silent-CPU-fallback
# shipping incident.
if(_nereus_gpu_spectrum_requested AND NOT NEREUS_GPU_SPECTRUM)
    message(FATAL_ERROR
        "NEREUS_GPU_SPECTRUM was requested but cannot be enabled.\n"
        "Reason: ${_nereus_gpu_fail_reason}\n"
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
        "with -DNEREUS_GPU_SPECTRUM=OFF.")
endif()

# --------------------------------------------------------------------------
# Source files
# --------------------------------------------------------------------------
set(CORE_SOURCES
    src/core/AntennaLabels.cpp
    src/core/AppSettings.cpp
    src/core/BoardCapabilities.cpp
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
    src/core/SupportBundle.cpp
    src/core/RadioDiscovery.cpp
    src/core/RadioConnection.cpp
    src/core/TciProtocol.cpp
    src/core/TciSendQueue.cpp
    src/core/TciVfoCoalescer.cpp
    src/core/TciBinaryFrame.cpp
    src/core/TciServer.cpp
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
    src/core/ReceiverManager.cpp
    src/core/AudioEngine.cpp
    src/core/WdspEngine.cpp
    src/core/RxChannel.cpp
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
    src/core/TxSliceArbiter.cpp
    src/core/TxSliceArbiter.h
    src/core/FFTEngine.cpp
    src/core/NoiseFloorEstimator.cpp
    src/core/NoiseFloorTracker.cpp
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
    src/core/safety/SwrProtectionController.cpp
    src/core/safety/TxInhibitMonitor.cpp
    src/core/safety/safety_constants.cpp
    src/core/SpotCollectorClient.cpp
    src/core/PotaClient.cpp
    src/core/DxClusterClient.cpp
    src/core/WsjtxClient.cpp
    src/core/FreeDVReporterClient.cpp
    src/core/FreeDVRadeReporterBridge.cpp
    src/core/PskReporterClient.cpp
    src/core/CtyDatParser.cpp
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
    # R1 Task 3: SpectrumDetector/SpectrumAvenger relocated from
    # src/gui/spectrum/ -- verbatim WDSP analyzer.c ports with no GUI
    # dependency, needed by the headless daemon build split.
    src/core/spectrum/SpectrumDetector.cpp
    src/core/spectrum/SpectrumAvenger.cpp
    # R1 Task 5: crop-and-reduce extracted from SpectrumWidget.  Takes an
    # explicit pixel count and an explicit (centreHz, spanHz) window rather
    # than reading widget geometry and widget member state, so the daemon
    # can run the same reduction with no widget in the process.
    src/core/spectrum/SpectrumReducer.cpp
    # R1 Task 6: per-stream FFTEngine pool extracted from
    # MainWindow::createFftEngineForStream / m_fftEngines / m_fftThread.
    # Owns engine lifecycle, the four global display AppSettings-sourced
    # knobs (as FftPoolConfig), and the shared worker thread(s), so the
    # daemon can produce spectrum with no widget toolkit in the process.
    src/core/spectrum/FftEnginePool.cpp
    # R1 Task 7: consumer-to-stream subscription set extracted from the
    # router-mutation half of MainWindow::rebuildFftRouting. Rebuilds an
    # FFTRouter from its own subscription set on demand, so the daemon can
    # drive the same router from remote-endpoint ids with no PanadapterStack
    # (a QWidget) in the process.
    src/core/spectrum/FftTopology.cpp
    # R1 Task 8: shared startup sequence (settings load, one-shot schema
    # migrations, LogManager restore, file-logging install) extracted from
    # main(), each of which used to have exactly one call site. Lets
    # server_main.cpp (R1 Task 9) run the identical sequence instead of a
    # daemon-first install running against an unmigrated settings store.
    src/core/CoreInit.cpp
    # R1 Task 9: nereusd's own "key = value" config file parser (default
    # /etc/nereusd.conf). Widget-free by construction (QString/QFile/
    # QTextStream only) so it links into nereusd alongside NereusCore with
    # no NereusGui dependency.
    src/core/daemon/DaemonConfig.cpp
    # R1 Task 10: connects the daemon to a radio (or runs discovery) and
    # tops the slice list up to min(cfg.sliceCount, connected-board-
    # maxSlices) using RadioModel's own addSlice() -- the same entry
    # point RadioModel::connectToRadio() uses for Slice A, not the
    # GUI-only addSliceOnPan(). Includes models/RadioModel.h (declared in
    # MODEL_SOURCES below); CMake source-list variables are plain lists
    # evaluated at generate time, so this forward reference to a set
    # declared later in the file is fine -- both lists end up as sources
    # on the same NereusCore target either way (see the add_library
    # call further down).
    src/core/daemon/DaemonApp.cpp
)

# CoreAudioHalBus — macOS-only IAudioBus implementation bridging to the
# NereusSDRVAX HAL plugin via POSIX shared memory. The header is
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
    src/models/RadioModel.cpp
    src/models/RxDspWorker.cpp
    src/models/Band.cpp
    src/models/BandDefaults.cpp
    src/models/BandPlanManager.cpp
    src/models/SliceModel.cpp
    src/models/PanadapterModel.cpp
    src/models/MeterModel.cpp
    src/models/TransmitModel.cpp
    src/models/FilterPresetStore.cpp
    src/models/SpotModel.cpp
    src/models/SpotTableModel.cpp
    src/models/BandFilterProxy.cpp
    src/models/FreeDVStationModel.cpp
    src/models/RxDecodeModel.cpp
)

set(GUI_SOURCES
    src/gui/MainWindow.cpp
    src/gui/AmpViewChart.cpp
    src/gui/AmpViewWindow.cpp
    src/gui/PsaIndicatorWidget.cpp
    src/gui/PsaIndicatorWidget.h
    src/gui/PsForm.cpp
    src/gui/TitleBar.cpp
    src/gui/RelayBar.cpp
    src/gui/SMeterWidget.cpp
    src/gui/ConnectionPanel.cpp
    src/gui/AddCustomRadioDialog.cpp
    src/gui/SupportDialog.cpp
    src/gui/AboutDialog.cpp
    src/gui/VaxFirstRunDialog.cpp
    src/gui/spectrum/ActivePeakHoldTrace.cpp
    src/gui/spectrum/PeakBlobDetector.cpp
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
    src/gui/widgets/CenterMarkSlider.h
    src/gui/widgets/DexpPeakMeter.h
    src/gui/widgets/DexpPeakMeter.cpp
    src/gui/widgets/DspParamPopup.cpp
    src/gui/widgets/FilterPassbandWidget.cpp
    src/gui/widgets/FilterPresetEditDialog.cpp
    src/gui/widgets/GuardedComboBox.h
    src/gui/widgets/GuardedSlider.h
    src/gui/widgets/MeterSlider.h
    src/gui/widgets/MeterSlider.cpp
    src/gui/widgets/OcLedStripWidget.cpp
    src/gui/widgets/ParametricEqWidget.h
    src/gui/widgets/ParametricEqWidget.cpp
    src/gui/widgets/ResetSlider.h
    src/gui/widgets/ScrollableLabel.cpp
    src/gui/widgets/TriBtn.h
    src/gui/widgets/TriBtn.cpp
    src/gui/widgets/VfoLevelBar.cpp
    src/gui/widgets/VfoModeContainers.cpp
    src/gui/widgets/VfoModeContainers.h
    src/gui/widgets/VaxChannelSelector.cpp
    src/gui/widgets/MasterOutputWidget.cpp
    src/gui/widgets/VfoStyles.h
    src/gui/widgets/VfoWidget.cpp
    src/gui/widgets/MetricLabel.cpp
    src/gui/widgets/MetricLabel.h
    src/gui/widgets/StatusBadge.cpp
    src/gui/widgets/StatusBadge.h
    src/gui/widgets/AdcOverloadBadge.cpp
    src/gui/widgets/AdcOverloadBadge.h
    src/gui/widgets/OverflowChip.cpp
    src/gui/widgets/OverflowChip.h
    src/gui/widgets/BadgePair.cpp
    src/gui/widgets/BadgePair.h
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
    src/gui/FreeDVReporterDialog.cpp
    src/gui/FreeDVReporterDialog.h
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
    src/gui/setup/PgxlAdvancedPage.cpp
    src/gui/setup/TgxlAdvancedPage.cpp
    src/gui/setup/PgxlInterlockPage.cpp
    src/gui/setup/FourO3APage.cpp
    src/gui/setup/RfKitPage.cpp
    src/gui/setup/TciLogWindow.cpp
    src/gui/setup/KeyboardSetupPages.cpp
    src/gui/setup/DiagnosticsSetupPages.cpp
    src/gui/diagnostics/RadioStatusPage.cpp
    src/gui/diagnostics/DiagnosticsPhaseHPages.cpp
    src/gui/applets/AppletWidget.cpp
    src/gui/applets/NyiOverlay.cpp
    src/gui/applets/AppletPanelWidget.cpp
    src/gui/applets/AppletVisibilityController.cpp
    src/gui/applets/RxApplet.cpp
    src/gui/applets/TxApplet.cpp
    src/gui/applets/TxEqDialog.cpp
    src/gui/applets/TxCfcDialog.cpp
    src/gui/applets/PhoneCwApplet.cpp
    src/gui/applets/RadeApplet.cpp
    src/gui/applets/EqApplet.cpp
    src/gui/applets/VaxApplet.cpp
    src/gui/applets/FmApplet.cpp
    src/gui/applets/DigitalApplet.cpp
    src/gui/applets/PureSignalApplet.cpp
    src/gui/applets/DiversityApplet.cpp
    src/gui/applets/CwxApplet.cpp
    src/gui/applets/DvkApplet.cpp
    src/gui/applets/CatApplet.cpp
    src/gui/applets/TunerApplet.cpp
    src/gui/applets/AmpApplet.cpp
    src/gui/applets/Rf2ksApplet.cpp
    src/gui/applets/TciApplet.cpp
    src/gui/applets/ClientChainApplet.cpp
)

# --------------------------------------------------------------------------
# Resources
# --------------------------------------------------------------------------
qt_add_resources(RESOURCES resources.qrc)

# --------------------------------------------------------------------------
# App icon (per-platform) — from main (PR #11)
# --------------------------------------------------------------------------
set(APP_ICON_SOURCES "")
if(APPLE)
    set(APP_ICON_MACOS "${CMAKE_SOURCE_DIR}/resources/icons/NereusSDR.icns")
    set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    list(APPEND APP_ICON_SOURCES ${APP_ICON_MACOS})
elseif(WIN32)
    list(APPEND APP_ICON_SOURCES "${CMAKE_SOURCE_DIR}/resources/icons/NereusSDR.rc")
endif()

# --------------------------------------------------------------------------
# Object library — shared between the app executable and test targets so
# the same translation units are compiled once and consumed by both
# (Phase 3I test infrastructure). Properties on NereusSDRObjs propagate
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
# Split for the headless daemon (docs/architecture/2026-07-28-remote-daemon-architecture-design.md
# section 4.1).  NereusCore must reference no widget symbol so nereusd can link
# it alone.  NereusSDRObjs survives as an INTERFACE aggregate because 589
# nereus_add_test() call sites link it by name and reuse its PCH.
add_library(NereusCore OBJECT
    ${CORE_SOURCES}
    ${MODEL_SOURCES}
)

add_library(NereusGui OBJECT
    ${GUI_SOURCES}
    ${RESOURCES}
)

# Linux-only sources: the PipeWire/pactl first-run + diagnose dialog uses
# POSIX <unistd.h> (getuid) and the LinuxAudioBackend probe. MainWindow.cpp
# already gates the menu wiring on Q_OS_LINUX, so the .cpp shouldn't enter
# the build on macOS or Windows.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(NereusGui PRIVATE src/gui/VaxLinuxFirstRunDialog.cpp)
endif()

add_library(NereusSDRObjs INTERFACE)
target_link_libraries(NereusSDRObjs INTERFACE NereusCore NereusGui)
# target_link_libraries() alone propagates NereusCore/NereusGui's USAGE
# REQUIREMENTS (link libraries, include dirs, compile definitions) to
# whatever links NereusSDRObjs, but does NOT propagate their compiled
# object files: CMake's "linking an OBJECT library adds its objects to my
# sources" behavior only triggers when a target names the OBJECT library
# directly in its own target_link_libraries call, not through another
# level of indirection. Verified by an actual link failure (every symbol
# from both object libraries came back undefined) before this line was
# added. Declaring the objects as INTERFACE sources here is what actually
# makes every existing target_link_libraries(x PRIVATE NereusSDRObjs) call
# site link real code.
target_sources(NereusSDRObjs INTERFACE
    $<TARGET_OBJECTS:NereusCore>
    $<TARGET_OBJECTS:NereusGui>
)

# 2026-05-26 KG4VCF: parallel LTO-enabled object library pair, ONLY built
# when NEREUSSDR_ENABLE_LTO=ON.  Same sources as NereusCore/NereusGui above,
# but compiled with LTO bitcode so the main NereusSDR app's link can
# do cross-translation-unit optimization.  Tests continue to link the
# plain NereusSDRObjs aggregate (no bitcode), so test link times stay fast.
#
# This is more work to build a release artifact (sources compile
# twice) but it keeps the dev loop fast while preserving the LTO perf
# benefit for shipped binaries.
#
# NereusCore_LTO/NereusGui_LTO independently recompile the same source
# lists rather than linking their non-LTO twins PUBLIC: linking, say,
# NereusCore_LTO to NereusCore would pull NereusCore's already-compiled
# objects into the same link as NereusCore_LTO's own copies of the same
# translation units, producing duplicate symbols. The nereus_apply_*_deps()
# functions defined below are called once per twin so all four targets'
# dependency sets stay identical without that link.
if(NEREUSSDR_LTO_AVAILABLE)
    add_library(NereusCore_LTO OBJECT
        ${CORE_SOURCES}
        ${MODEL_SOURCES}
    )
    add_library(NereusGui_LTO OBJECT
        ${GUI_SOURCES}
        ${RESOURCES}
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_sources(NereusGui_LTO PRIVATE src/gui/VaxLinuxFirstRunDialog.cpp)
    endif()
    foreach(_nereus_lto_target NereusCore_LTO NereusGui_LTO)
        set_target_properties(${_nereus_lto_target} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION TRUE
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    endforeach()
endif()

add_executable(NereusSDR src/main.cpp ${APP_ICON_SOURCES})
if(NEREUSSDR_LTO_AVAILABLE)
    # Main app + its OBJECT-lib twins link both get LTO when enabled.  The
    # IPO property on the executable arranges -flto on the link line;
    # the IPO property on NereusCore_LTO/NereusGui_LTO above puts -flto on
    # every compile.  Together: full cross-TU optimization for the shipped
    # binary.
    target_link_libraries(NereusSDR PRIVATE NereusCore_LTO NereusGui_LTO)
    set_target_properties(NereusSDR PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION TRUE
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
else()
    target_link_libraries(NereusSDR PRIVATE NereusSDRObjs)
endif()

set_target_properties(NereusSDR PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE "NereusSDR.icns"
    MACOSX_BUNDLE_BUNDLE_NAME "NereusSDR"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.boydsoftprez.NereusSDR"
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
    set(_nereus_mic_desc "NereusSDR uses the microphone for SSB voice transmission and live mic level monitoring.")
    add_custom_command(TARGET NereusSDR POST_BUILD
        COMMAND /bin/sh -c "/usr/libexec/PlistBuddy -c \"Set :NSMicrophoneUsageDescription '${_nereus_mic_desc}'\" \"$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Info.plist\" 2>/dev/null || /usr/libexec/PlistBuddy -c \"Add :NSMicrophoneUsageDescription string '${_nereus_mic_desc}'\" \"$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Info.plist\""
        COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:NereusSDR>"
        COMMENT "Inject NSMicrophoneUsageDescription + re-sign bundle (mic-permission gate)"
        VERBATIM
    )
endif()

# --------------------------------------------------------------------------
# nereusd -- headless daemon (R1 Task 9; see
# docs/architecture/2026-08-02-remote-daemon-r1-plan.md Task 9 and
# docs/architecture/2026-07-28-remote-daemon-architecture-design.md section 4).
#
# Links NereusCore ONLY: no NereusGui, no NereusSDRObjs, no LTO twin. This is
# the acceptance test the whole R1 CMake split (Task 1) exists to make
# possible -- a build failure here on a widget symbol means an extraction
# from Tasks 2 through 7 is incomplete and must be fixed at the source, not
# papered over by linking NereusGui. A plain console executable (no
# WIN32_EXECUTABLE, no MACOSX_BUNDLE): it has no window to show, and a Pi
# running it under systemd wants stdout/stderr and an exit code, not an
# app bundle. No LTO twin for the same reason NereusSDR gets one and this
# does not yet: nereusd is a new, still-growing binary (R1 Tasks 10-13 add
# to it), and NereusCore/NereusCore_LTO's dependency sets are kept identical
# by nereus_apply_core_deps() below regardless, so adding one is a
# non-behavioural follow-up whenever the daemon's own performance profile
# asks for it, not a Task 9 concern.
# --------------------------------------------------------------------------
add_executable(nereusd src/server_main.cpp)
target_link_libraries(nereusd PRIVATE NereusCore)

# --------------------------------------------------------------------------
# NereusCore / NereusGui dependency application (R1 Task 1; see
# docs/architecture/2026-07-28-remote-daemon-architecture-design.md section 4.1)
#
# NereusCore, NereusGui, and their LTO twins are four distinct OBJECT
# library targets. Rather than hand-duplicating each target_link_libraries /
# target_include_directories / target_compile_definitions call for all four,
# the dependency set each half needs is hoisted into the two functions below
# and applied by calling them once per target. NereusGui/NereusGui_LTO see
# the NereusCore dependencies transitively because they link their
# NereusCore twin PUBLIC (below).
#
# Qt6::Svg is GUI-only. Qt6::Widgets and Qt6::GuiPrivate are still on
# NereusCore too, despite being the two names the R1 plan expected to be
# GUI-only. R1 Task 4 removed the #include chain that used to force this
# (src/models/RadioModel.cpp no longer reaches gui/SpectrumWidget.h), but
# the links themselves stay here on purpose: dropping them is a separate
# build-and-fix pass that has to verify nothing else compiled into
# NereusCore still needs Qt Widgets or GuiPrivate transitively, and
# removing them without that verification could break the app link in
# ways a source-only change cannot catch. See the comments in
# nereus_apply_core_deps() for detail.
# --------------------------------------------------------------------------
function(nereus_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W3 /Zc:__cplusplus)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_compile_options(${target} PUBLIC -fsanitize=address)
        endif()
    endif()
endfunction()

function(nereus_apply_core_deps target)
    target_include_directories(${target} PUBLIC
        ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(${target} PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Multimedia
        ZLIB::ZLIB
    )

    # Qt6::Widgets is NOT GUI-only in this codebase today, however much it
    # should be. R1 Task 4 ("Extract ISpectrumSink from RadioModel") has
    # landed and removed the reason this link used to be load-bearing:
    # RadioModel.cpp no longer includes gui/SpectrumWidget.h, and its former
    # direct m_spectrumWidget->setHighSwrOverlay(...) calls now go through
    # the abstract NereusSDR::ISpectrumSink interface instead. The link
    # stays here anyway, kept deliberately rather than dropped as a
    # drive-by: nobody has yet verified, with an actual build, that nothing
    # else compiled into NereusCore still needs Qt6::Widgets transitively.
    # That verification, and the removal if it holds, is a separate
    # follow-up task's job.
    target_link_libraries(${target} PUBLIC Qt6::Widgets)

    if(APPLE)
        # AVFoundation provides AVCaptureDevice for the launch-time mic-permission
        # prompt (src/core/MacMicPermission.mm, issue #203).
        target_link_libraries(${target} PUBLIC "-framework AVFoundation")

        # CoreAudio framework for AudioObjectGetPropertyData +
        # kAudioDevicePropertyIOThreadOSWorkgroup used by
        # src/core/audio/RealtimeAudioPriority.cpp.  The os_workgroup_*
        # APIs themselves come from libSystem (already linked) but the
        # property-query helpers and AudioDeviceID type live in CoreAudio.
        target_link_libraries(${target} PUBLIC "-framework CoreAudio")
    endif()

    if(WIN32)
        # avrt provides AvSetMmThreadCharacteristicsW + AvRevertMmThreadCharacteristics
        # used by src/core/audio/RealtimeAudioPriority.cpp to join the
        # "Pro Audio" MMCSS task class.
        target_link_libraries(${target} PUBLIC avrt)
    endif()

    if(Qt6SerialPort_FOUND)
        target_link_libraries(${target} PUBLIC Qt6::SerialPort)
        target_compile_definitions(${target} PUBLIC HAVE_SERIALPORT)
    endif()

    # Qt6::WebSockets is REQUIRED above; the link and HAVE_WEBSOCKETS define
    # are unconditional. The #ifdef HAVE_WEBSOCKETS guards in TciServer,
    # FreeDVReporterClient, MainWindow, and SpotHubDialog are now redundant
    # but left in place to keep this PR surgical; cleanup tracked as a
    # follow-up.
    target_link_libraries(${target} PUBLIC Qt6::WebSockets)
    target_compile_definitions(${target} PUBLIC HAVE_WEBSOCKETS)

    if(FFTW3_FOUND)
        target_include_directories(${target} PUBLIC ${FFTW3_INCLUDE_DIRS})
        target_link_libraries(${target} PUBLIC ${FFTW3_LIBRARIES})
        target_compile_definitions(${target} PUBLIC HAVE_FFTW3)
    endif()

    # libpipewire-0.3, gated by the pkg_check_modules block above. Only links on
    # Linux when the dev package is present; NEREUS_HAVE_PIPEWIRE lets follow-up
    # PipeWire sources compile conditionally.
    if(PIPEWIRE_FOUND)
        target_link_libraries(${target} PUBLIC ${PIPEWIRE_LIBRARIES})
        target_include_directories(${target} PUBLIC ${PIPEWIRE_INCLUDE_DIRS})
        target_compile_options(${target} PUBLIC ${PIPEWIRE_CFLAGS_OTHER})
        target_compile_definitions(${target} PUBLIC NEREUS_HAVE_PIPEWIRE)
        target_sources(${target} PRIVATE
            src/core/audio/PipeWireThreadLoop.cpp
            src/core/audio/PipeWireStream.cpp
            src/core/audio/PipeWireBus.cpp
        )
    endif()

    if(WDSP_FOUND)
        target_link_libraries(${target} PUBLIC wdsp_static)
        target_compile_definitions(${target} PUBLIC HAVE_WDSP)
    endif()

    target_link_libraries(${target} PUBLIC rade)
    target_include_directories(${target} PUBLIC
        ${CMAKE_SOURCE_DIR}/third_party/rade/src
        ${CMAKE_BINARY_DIR}/third_party/rade/build_opus-prefix/src/build_opus/include
        ${CMAKE_BINARY_DIR}/third_party/rade/build_opus-prefix/src/build_opus/dnn
        ${CMAKE_BINARY_DIR}/third_party/rade/build_opus-prefix/src/build_opus/celt
        ${CMAKE_BINARY_DIR}/third_party/rade/build_opus-prefix/src/build_opus
    )

    target_link_libraries(${target} PUBLIC r8brain)

    if(ENABLE_DFNR)
        target_sources(${target} PRIVATE src/core/DeepFilterFilter.cpp)
        target_include_directories(${target} PRIVATE ${DEEPFILTER_DIR}/include)
        target_link_libraries(${target} PRIVATE ${DFNR_LIB})
        # PUBLIC, not PRIVATE: src/gui/setup/DspSetupPages.cpp checks
        # #ifdef HAVE_DFNR to decide whether to show the real DeepFilter NR
        # controls or a "not compiled in" note, and DspSetupPages.cpp
        # compiles into NereusGui. The define has to cross the
        # NereusGui->NereusCore link, so it cannot stay PRIVATE the way it
        # was on the single pre-split object library.
        target_compile_definitions(${target} PUBLIC HAVE_DFNR)
    endif()

    if(ENABLE_MNR AND APPLE)
        target_sources(${target} PRIVATE src/core/MacNRFilter.cpp)
        target_link_libraries(${target} PUBLIC "-framework Accelerate")
        target_compile_definitions(${target} PUBLIC HAVE_MNR)
    endif()

    # PortAudio (Phase 3O VAX): always linked; PortAudioBus is the cross-platform
    # IAudioBus fallback when no native low-latency backend is available.
    target_link_libraries(${target} PRIVATE portaudio_static)
    target_include_directories(${target} PUBLIC ${portaudio_SOURCE_DIR}/include)

    if(NEREUS_GPU_SPECTRUM)
        target_compile_definitions(${target} PUBLIC NEREUS_GPU_SPECTRUM)

        # Same history as the Qt6::Widgets link above: this used to be
        # load-bearing only because RadioModel.cpp (a NereusCore source)
        # reached gui/SpectrumWidget.h, a QRhiWidget subclass that pulls in
        # <rhi/qrhi.h> (Qt6::GuiPrivate) whenever GPU spectrum is enabled.
        # R1 Task 4 removed that include. The link is kept here for the
        # same reason as Qt6::Widgets above, pending the same follow-up
        # verification before either is dropped.
        target_link_libraries(${target} PUBLIC Qt6::GuiPrivate)
    endif()

    target_compile_definitions(${target} PUBLIC
        NEREUSSDR_VERSION="${PROJECT_VERSION}"
    )

    nereus_apply_warnings(${target})
endfunction()

function(nereus_apply_gui_deps target)
    # Qt6::Widgets and Qt6::GuiPrivate are NOT listed here: both are
    # NereusCore dependencies for now (see the comments in
    # nereus_apply_core_deps()), and NereusGui/NereusGui_LTO see them
    # transitively through the NereusCore PUBLIC link below.
    target_link_libraries(${target} PUBLIC
        Qt6::Svg
    )

    nereus_apply_warnings(${target})
endfunction()

if(WIN32)
    # Needed by RadioDiscovery.cpp's setsockopt() call for SO_BROADCAST.
    target_link_libraries(NereusSDR PRIVATE ws2_32)
endif()

if(WDSP_FOUND)
    # Sub-epic C-1 NR backends: rnnoise (NR3/rnnr.c) and libspecbleach (NR4/sbnr.c).
    # Must be add_subdirectory'd before wdsp so the targets exist when
    # third_party/wdsp/CMakeLists.txt pulls them in as PUBLIC link deps.
    # wdsp_static is linked from nereus_apply_core_deps(), called below.
    add_subdirectory(third_party/rnnoise)
    add_subdirectory(third_party/libspecbleach)
    add_subdirectory(third_party/wdsp)
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
# We re-expose them as target-scope include dirs on NereusCore (via
# nereus_apply_core_deps(), called below) so any future NereusSDR consumer
# that #includes "rade_api.h" can resolve the transitive Opus headers.
# --------------------------------------------------------------------------
add_subdirectory(third_party/rade)

if(NOT TARGET rade)
    message(FATAL_ERROR "RADE library target 'rade' not found after add_subdirectory(third_party/rade)")
endif()

# rade is a SHARED library on Windows (rade.dll). NereusSDR.exe load-time
# imports it via __declspec(dllimport), so without rade.dll on the DLL
# search path the .exe fails to launch with "rade.dll was not found".
# CMake places rade.dll in third_party/rade/src/ but the .exe lives at
# $<TARGET_FILE_DIR:NereusSDR>, so copy it next to the .exe at build time.
# Mirrors the ENABLE_DFNR / deepfilter.dll pattern below.
if(WIN32)
    add_custom_command(TARGET NereusSDR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:rade>" "$<TARGET_FILE_DIR:NereusSDR>"
        COMMENT "Copying rade.dll to build directory")
endif()

# --------------------------------------------------------------------------
# r8brain-free-src high-quality polyphase resampler (Phase 3R Task I2)
# License: MIT (see third_party/r8brain/LICENSE.txt)
# Vendored at SHA 5c44bebe9c477d47b1dc7037fcaae2794ff2b4e1 (2026-03-12).
#
# r8brain is header-only; the OBJECT library exists to anchor the
# function-local statics in CDSPRealFFT.h to one translation unit and
# to expose the include directory to NereusSDR sources that #include
# "CDSPResampler.h".
# --------------------------------------------------------------------------
add_subdirectory(third_party/r8brain)

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
    # Sources / include dirs / link lib / HAVE_DFNR define applied by
    # nereus_apply_core_deps(), called below.
    if(WIN32)
        target_link_libraries(NereusSDR PRIVATE ws2_32 bcrypt userenv ntdll)
        if(DFNR_DLL AND EXISTS ${DFNR_DLL})
            add_custom_command(TARGET NereusSDR POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DFNR_DLL}" "$<TARGET_FILE_DIR:NereusSDR>"
                COMMENT "Copying deepfilter.dll to build directory")
        endif()
    elseif(APPLE)
        # Rust runtime deps on macOS
        target_link_libraries(NereusSDR PRIVATE "-framework Security" "-framework CoreFoundation")
    else()
        # Rust runtime deps on Linux
        target_link_libraries(NereusSDR PRIVATE pthread dl m)
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
            add_custom_command(TARGET NereusSDR POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_DIR:NereusSDR>/Contents/Resources/models/dfnet3"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DFNR_MODEL}" "$<TARGET_BUNDLE_DIR:NereusSDR>/Contents/Resources/models/dfnet3/"
                COMMENT "Copying DeepFilterNet3 model to app bundle Resources/models/dfnet3")
        else()
            # Non-macOS: place adjacent to exe in models/dfnet3/ subfolder so
            # ModelPaths::probeModel() probe #1 finds it at
            # <exeDir>/models/dfnet3/<file>.
            add_custom_command(TARGET NereusSDR POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:NereusSDR>/models/dfnet3"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DFNR_MODEL}" "$<TARGET_FILE_DIR:NereusSDR>/models/dfnet3/"
                COMMENT "Copying DeepFilterNet3 model to build directory models/dfnet3")
        endif()
        # Install the model alongside the binary for cmake --install.
        # ModelPaths probe #3 expects <exeDir>/../share/NereusSDR/models/dfnet3/<file>.
        if(NOT APPLE)
            install(FILES "${DFNR_MODEL}"
                DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/NereusSDR/models/dfnet3"
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
    # Sources / link / HAVE_MNR define applied by nereus_apply_core_deps(),
    # called below.
    message(STATUS "MNR (Apple Accelerate) enabled")
elseif(ENABLE_MNR)
    set(ENABLE_MNR OFF)
    message(STATUS "MNR (Apple Accelerate) disabled — macOS only")
endif()

nereus_apply_core_deps(NereusCore)
nereus_apply_gui_deps(NereusGui)
target_link_libraries(NereusGui PUBLIC NereusCore)

if(NEREUSSDR_LTO_AVAILABLE)
    nereus_apply_core_deps(NereusCore_LTO)
    nereus_apply_gui_deps(NereusGui_LTO)
    target_link_libraries(NereusGui_LTO PUBLIC NereusCore_LTO)
endif()

# GPU shader resources are attached to NereusCore only, not to
# NereusCore_LTO. qt_add_shaders()'s generated custom command uses an
# OUTPUT path derived from CMAKE_CURRENT_BINARY_DIR plus the shader's
# relative source path, not from the target name; calling it a second
# time for the same shader files from this same directory scope would
# register a second custom command for the same output file and fail the
# configure. This matches the pre-split object library exactly:
# NereusSDRObjs_LTO never carried the shader resource either, since
# qt_add_shaders(NereusSDRObjs ...) was the only call site.
if(NEREUS_GPU_SPECTRUM AND Qt6ShaderTools_FOUND)
    qt_add_shaders(NereusCore "nereus_shaders"
        PREFIX "/shaders"
        FILES
            resources/shaders/waterfall.vert
            resources/shaders/waterfall.frag
            resources/shaders/spectrum.vert
            resources/shaders/spectrum.frag
            resources/shaders/overlay.vert
            resources/shaders/overlay.frag
    )

    qt_add_shaders(NereusCore "nereus_meter_shaders"
        PREFIX "/shaders"
        FILES
            resources/shaders/meter_textured.vert
            resources/shaders/meter_textured.frag
            resources/shaders/meter_geometry.vert
            resources/shaders/meter_geometry.frag
    )
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
# Release artifacts get an empty tag and keep the clean "NereusSDR 0.5.2"
# title. Detection needs no CI change: release.yml checks out `ref: <tag>`
# (.github/workflows/release.yml), so HEAD sits exactly on a tag there and
# nowhere else.
#
# The derivation itself lives in cmake/NereusBuildTag.cmake and runs once per
# build, not once per configure. It used to run here, which meant the tag
# froze at whatever HEAD was when the build tree was last configured; a bench
# build was observed advertising a sha five commits stale. The script writes
# build/generated/NereusBuildTag.h only when the text changes, so the extra
# build step costs one git call and recompiles nothing on a no-op build.
set(NEREUSSDR_BUILD_TAG "" CACHE STRING
    "Text appended to the window title identifying a non-release build. Auto-derived from git when empty; set to a single space to force it off.")

set(NEREUS_BUILD_TAG_HEADER "${CMAKE_BINARY_DIR}/generated/NereusBuildTag.h")
set(NEREUS_BUILD_TAG_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NereusBuildTag.cmake")

# Resolve git once here rather than in the script, which would repeat the
# find_program on every build. Copied into a local rather than clearing
# GIT_EXECUTABLE itself: that is a cache variable, and a directory-scope
# shadow of it would be inherited by add_subdirectory(tests) and
# add_subdirectory(hal-plugin) below.
find_package(Git QUIET)
set(NEREUS_BUILD_TAG_GIT "")
if(GIT_FOUND)
    set(NEREUS_BUILD_TAG_GIT "${GIT_EXECUTABLE}")
endif()

set(NEREUS_BUILD_TAG_ARGS
    "-DNEREUS_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DNEREUS_OUTPUT=${NEREUS_BUILD_TAG_HEADER}"
    "-DNEREUS_OVERRIDE=${NEREUSSDR_BUILD_TAG}"
    "-DNEREUS_GIT_EXECUTABLE=${NEREUS_BUILD_TAG_GIT}")

# Configure-time run: the header has to exist before the first compile (and
# before AUTOMOC scans main.cpp), and this is where the one-line smoke-vs-
# release status message comes from.
execute_process(
    COMMAND "${CMAKE_COMMAND}" ${NEREUS_BUILD_TAG_ARGS} -DNEREUS_ANNOUNCE=ON
            -P "${NEREUS_BUILD_TAG_SCRIPT}")

# Build-time run: always out of date on purpose, so the tag tracks HEAD
# without needing a re-configure.
add_custom_target(nereus_build_tag ALL
    COMMAND "${CMAKE_COMMAND}" ${NEREUS_BUILD_TAG_ARGS}
            -P "${NEREUS_BUILD_TAG_SCRIPT}"
    BYPRODUCTS "${NEREUS_BUILD_TAG_HEADER}"
    COMMENT "Re-deriving the smoke-build tag"
    VERBATIM)
add_dependencies(NereusSDR nereus_build_tag)

# Deliberately PRIVATE to the executable, and deliberately consumed only by
# src/main.cpp. Every one of the 589 test executables links NereusCore,
# so a generated header reaching that object library would relink all of them
# each time HEAD moved. Confined here, a new commit recompiles one small
# translation unit and relinks one binary. main.cpp hands the value to
# BuildIdentity, which is what MainWindow reads when it composes the title.
target_include_directories(NereusSDR PRIVATE "${CMAKE_BINARY_DIR}/generated")

# --------------------------------------------------------------------------
# Version define + compiler warnings
#
# Both are applied via nereus_apply_core_deps() / nereus_apply_warnings()
# above (called once per OBJECT library twin: NereusCore, NereusGui, and
# their LTO variants). NereusSDRObjs is now a source-less INTERFACE
# aggregate and cannot carry PUBLIC/PRIVATE compile definitions or options
# directly, so those calls moved to the concrete targets.
#
# The one exception is the address-sanitizer LINK flag below: main.cpp is
# compiled directly into the NereusSDR executable target (not into
# NereusCore/NereusGui), so it needs its own -fsanitize=address on the
# link line to match the compile-time instrumentation nereus_apply_warnings()
# already gives NereusCore/NereusGui in Debug builds.
# --------------------------------------------------------------------------
if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(NereusSDR PRIVATE -fsanitize=address)
endif()

# --------------------------------------------------------------------------
# Precompiled header (NEREUS_USE_PCH=ON by default)
#
# CI bake-off (ubuntu-24.04, 4 vCPU, 451 test executables) showed Linux
# spending ~33m in the build step vs ~6m on macOS / ~12m on Windows.
# Most of that is repeated parsing of Qt6 + STL headers across the
# NereusCore translation units AND across every test target that
# REUSE_FROMs this PCH (see tests/CMakeLists.txt nereus_add_test()).
#
# Keep the list conservative: only headers used by the vast majority of
# TUs. Adding rarely-used headers slows incremental builds because every
# PCH change retriggers all consumers.
#
# NereusGui reuses this PCH too (REUSE_FROM below). NereusGui accounts for
# 217 of the object libraries' 351 translation units, so the PCH needs to
# cover both to actually pay for itself; the "GUI commons" header set
# right below exists for exactly those TUs. Safe to reuse: GUI_SOURCES
# has zero .mm files, so the Objective-C++ SKIP_PRECOMPILE_HEADERS
# problem below (which is specific to MacMicPermission.mm) doesn't apply.
#
# ccache must be configured with sloppiness=pch_defines,time_macros for
# PCH'd compiles to be cacheable; CI sets this in the "Configure ccache"
# step. Local builds inherit ccache's defaults — fine for clean builds,
# may have lower hit rates on incremental until ~/.config/ccache/ccache.conf
# matches CI.
# --------------------------------------------------------------------------
option(NEREUS_USE_PCH "Use precompiled headers to speed up builds" ON)
# Phase 3J-1 closeout follow-up (2026-05-13): exclude Objective-C++ (.mm)
# translation units from the PCH.  The PCH is generated as plain C++; an
# Objective-C++ TU that tries to reuse it errors out with "Objective-C was
# disabled in precompiled file ... but is currently enabled".  Affects
# MacMicPermission.mm on macOS.  set_source_files_properties with
# SKIP_PRECOMPILE_HEADERS bypasses the include for that single file
# without disabling PCH for the whole target.
if(APPLE)
    set_source_files_properties(src/core/MacMicPermission.mm
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()
if(NEREUS_USE_PCH)
    target_precompile_headers(NereusCore PRIVATE
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
        # GUI commons (every applet + dialog + widget TU). Still resolves
        # on NereusCore: Qt6::Widgets stays a NereusCore link even after R1
        # Task 4's ISpectrumSink extraction landed. See
        # nereus_apply_core_deps()'s Qt6::Widgets comment for why the link
        # was kept rather than dropped alongside the include it used to
        # justify.
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
    target_precompile_headers(NereusGui REUSE_FROM NereusCore)
endif()

# --------------------------------------------------------------------------
# Install
# --------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS NereusSDR
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION .
)

# nereusd -- headless daemon (R1 Task 12; see
# docs/architecture/2026-08-02-remote-daemon-r1-plan.md Task 12).
install(TARGETS nereusd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Ask pkg-config for systemd's own vendor unit directory when
    # systemd.pc is available (the project's own dependency lists in
    # CLAUDE.md do not require libsystemd-dev, so this is not guaranteed);
    # fall back to the FHS-standard /usr/lib/systemd/system otherwise,
    # which is what every mainstream distro in a merged-/usr layout
    # (Debian, Ubuntu, Fedora, Arch, RHEL) ships as the vendor unit
    # directory.
    #
    # This is a packaging-convention choice, not a correctness fix.
    # systemd's documented system-unit load path also includes the
    # CMAKE_INSTALL_PREFIX-relative /usr/local/lib/systemd/system
    # (confirmed against Debian 13 trixie's own systemd.unit(5), this
    # project's named target distro per design doc section 4.5a), so
    # installing there under this project's default /usr/local prefix
    # would still be found by `systemctl enable` and would still work --
    # an earlier version of this comment claimed otherwise; that claim
    # was wrong (task-12-report.md, fix round 1, Finding 1). Resolving
    # the real vendor directory via pkg-config is simply the conventional
    # choice: systemdsystemunitdir is what systemd itself publishes as
    # the correct install location for this distro, so following it means
    # the unit lands wherever the target distro actually puts vendor
    # units, rather than wherever this project's CMAKE_INSTALL_PREFIX
    # happens to point. (A later version of this paragraph additionally
    # claimed a priority/precedence relationship between the two paths;
    # that claim was also wrong, in the opposite direction, and has been
    # removed rather than re-attempted -- see task-12-report.md, fix
    # round 2, Finding 1. The rationale above needs no ordering claim and
    # stands without one.)
    set(NEREUSD_SYSTEMD_UNIT_DIR "/usr/lib/systemd/system")
    if(PkgConfig_FOUND)
        pkg_check_modules(SYSTEMD QUIET systemd)
        if(SYSTEMD_FOUND)
            pkg_get_variable(_nereusd_systemd_unit_dir systemd systemdsystemunitdir)
            if(_nereusd_systemd_unit_dir)
                set(NEREUSD_SYSTEMD_UNIT_DIR "${_nereusd_systemd_unit_dir}")
            endif()
        endif()
    endif()
    message(STATUS "nereusd: systemd unit -> ${NEREUSD_SYSTEMD_UNIT_DIR}")

    install(FILES packaging/nereusd.service
            DESTINATION "${NEREUSD_SYSTEMD_UNIT_DIR}")
    install(FILES packaging/nereusd.conf.sample
            DESTINATION share/nereusd)
endif()

# Bundle third-party license notices + NereusSDR's LICENSE into release artifacts.
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
# Tests (Qt6::Test). Opt-in via -DNEREUS_BUILD_TESTS=ON.
# --------------------------------------------------------------------------
option(NEREUS_BUILD_TESTS "Build NereusSDR test suite" OFF)
if(NEREUS_BUILD_TESTS)
    # Propagate the define to the object library so test-guard blocks
    # (e.g. isTabVisibleForTest) are compiled into NereusCore and
    # therefore available for linking into test executables.
    target_compile_definitions(NereusCore PUBLIC NEREUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --------------------------------------------------------------------------
# macOS HAL Audio Server Plug-In (Phase 3O VAX — Sub-Phase 5)
#
# Builds NereusSDRVAX.driver, a standalone CoreAudio HAL plugin that
# exposes 4 virtual RX devices ("NereusSDR VAX 1..4") plus 1 TX device
# ("NereusSDR TX"). Off by default because the plugin pulls libASPL via
# FetchContent and its preferred build is a separate cmake tree
# (`cmake -B build-hal -S hal-plugin`) — see docs/architecture/
# 2026-04-19-vax-design.md §8.1. Enable here only when you want the
# driver target wired into the main configure for IDE integration.
# --------------------------------------------------------------------------
option(NEREUSSDR_BUILD_HAL_PLUGIN "Build macOS CoreAudio HAL Audio Server plug-in" OFF)
if(APPLE AND NEREUSSDR_BUILD_HAL_PLUGIN)
    add_subdirectory(hal-plugin)
endif()
