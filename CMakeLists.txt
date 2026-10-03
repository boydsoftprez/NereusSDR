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

# NereusSDRLib is a SHARED library, and on ELF targets every object linked
# into a shared object must be position-independent.  CMake sets -fPIC
# automatically for the SHARED target's own sources, but NOT for the
# vendored static libraries linked into it, which are separate targets.
# Without this, x86-64 Linux fails at link with:
#
#   mold: error: libwdsp_static.a(iqc.c.o): R_X86_64_PC32 relocation
#         against symbol `ch' can not be used; recompile with -fPIC
#
# r8brain, rnnoise and libspecbleach already set the property themselves;
# wdsp_static, portaudio_static and the rade/opus/lpcnet targets did not.
# Setting it globally covers all of them and any vendored lib added later,
# rather than leaving a per-target trap for the next one.  Must precede
# every target definition, hence its position here.
#
# No effect on macOS (arm64 Mach-O is always PIC) or Windows (PE has no
# equivalent notion); the cost on x86-64 Linux is one reserved register.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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

# iPhone app Task 16 (D36): on Linux the Core advertises itself over Bonjour
# through the Avahi daemon's D-Bus interface, with Qt's own D-Bus module.
# It is part of qt6-base (the Rock and Pi 4 builder image has it, and
# libQt6Network already depends on libQt6DBus), so no package is added.
# Without it the Core still builds and announces over the LAN datagram.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Qt6 QUIET COMPONENTS DBus)
    if(Qt6DBus_FOUND)
        message(STATUS "Qt6::DBus found: Bonjour through Avahi enabled")
    else()
        message(STATUS "Qt6::DBus not found: the Core will not advertise itself over Bonjour")
    endif()
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
# OpenSSL libcrypto (Remote Daemon R2, Task 17 -- TLS certificate provisioning)
# --------------------------------------------------------------------------
# nereusd mints its own self-signed X.509 certificate on first run (a
# headless Pi has no domain name to get a CA-signed one for); Qt6 has no API
# that GENERATES a certificate (QSslCertificate/QSslKey only parse), so
# src/core/security/CertificateStore links libcrypto directly. This is the
# first OpenSSL dependency anywhere in this tree -- verified 2026-08-08 that
# neither "OpenSSL" nor "libcrypto" appeared anywhere in CMakeLists.txt or
# src/ before this block.
#
# REQUIRED (fix round 1 review, reversing an initial "optional-at-
# configure-time" decision). The reviewer's objection: the optional gate's
# benefit expires the moment a real caller exists. Task 18 either calls
# CertificateStore unguarded, so an OpenSSL-less machine would configure
# cleanly and then fail at LINK with an undefined constructor symbol -- a
# worse diagnostic, arriving later -- or Task 18 guards every call behind
# `#ifdef HAVE_OPENSSL_CRYPTO`, producing a nereusd that configures, builds,
# installs and starts, then silently cannot serve wss://. That second shape
# is the exact v0.5.0 Qt6::WebSockets bug reproduced verbatim (see that
# block's own comment: "if a feature is mandatory, the build system has to
# refuse to configure without it"), on the one feature this phase is named
# after. Both successor states are worse than a loud configure error today,
# so this is REQUIRED even though CertificateStore itself has no production
# caller in this commit.
#
# Version floor 3.0: CertificateStore.cpp uses the OpenSSL 3.0 one-call key
# generator EVP_RSA_gen().
#
# Per-platform acquisition. Five routes, one per (OS, arch, workflow)
# combination that actually needs OpenSSL:
#
# macOS, arm64 (both ci.yml and release.yml, native):
#   Homebrew's openssl@3 formula. Fix round 3 correction: a prior revision
#   of this comment (and CLAUDE.md's) called it "keg-only" -- it is not
#   (`brew info --json=v2 openssl@3` reports `"keg_only": false`, and
#   `/opt/homebrew/lib/libcrypto*.dylib` / `/opt/homebrew/include/openssl`
#   are live symlinks into the Cellar, verified 2026-08-08).
#
#   Fix round 4 correction: that same revision then credited the WRONG
#   mechanism for why find_package(OpenSSL) needs no extra hint --
#   pointing at this file's own `if(APPLE)` block (above, near the top),
#   which calls link_directories()/include_directories(). Those two
#   commands affect compile and link command lines; per CMake's own
#   find_path()/find_library() documentation (what a Find module like
#   FindOpenSSL.cmake calls internally), neither one is consulted by
#   find_package's search procedure at all. The keg-only correction
#   itself still holds; only the "why" was wrong. Verified instead,
#   for the two cases that actually matter here:
#     - ci.yml and release.yml both pass -DCMAKE_PREFIX_PATH="$(brew
#       --prefix)" explicitly on this row's Configure step, and
#       CMAKE_PREFIX_PATH genuinely is one of find_path/find_library's
#       documented search inputs (confirmed against CMake's own
#       find_path() docs, 2026-08-08).
#     - A bare local configure with no explicit CMAKE_PREFIX_PATH (every
#       manual `cmake -B build` run during this task) resolves it a
#       DIFFERENT way, verified empirically rather than assumed:
#       FindOpenSSL.cmake also tries pkg-config internally (visible in
#       CMakeCache.txt as the pkgcfg_lib__OPENSSL_*/__pkg_config_*
#       variables), and Homebrew's own `pkg-config` binary has
#       /opt/homebrew/lib/pkgconfig compiled into its default search
#       path -- `pkg-config --variable pc_path pkg-config` lists it, and
#       `pkg-config --exists openssl` succeeds even with PKG_CONFIG_PATH
#       explicitly unset. Neither CMAKE_PREFIX_PATH nor the PATH
#       environment variable is involved in this second case: both were
#       checked and are empty/uninvolved on this machine.
#   Do not hardcode a Homebrew Cellar/opt path regardless of which of
#   the two mechanisms above actually resolves it in a given context.
#
# macOS, x86_64 (release.yml's Intel cross-compile row only):
#   Fix round 3, Important B. That arm64 openssl@3 keg has no x86_64
#   slice, so the route above does not apply on this row at all -- source-
#   built instead, the same pattern release.yml already uses for FFTW
#   immediately above this row's Configure step (see that step's own
#   comment): this is an Apple Silicon runner producing an Intel artifact,
#   so no Homebrew keg for either library is ever usable here regardless
#   of keg-only status. OpenSSL 3.0.21, Configure target
#   darwin64-x86_64-cc (bakes in -arch x86_64 itself), -mmacosx-version-min
#   matching the row's deployment target, shared (matching FFTW's
#   --enable-shared choice on the same row, so packaging treats both the
#   same way), installed via `make install_sw` (skips docs) into a
#   build-local prefix, wired in via an explicit -DOPENSSL_ROOT_DIR on
#   that row only. This is the exact artifact-class bug the whole
#   REQUIRED finding is modelled on -- v0.5.0's Windows build shipped
#   without rade.dll; unfixed, this would have been the Intel DMG failing
#   to configure, or worse, linking an arm64 libcrypto into an x86_64
#   binary -- caught here, before a release, not after.
#
# Linux (all three workflows plus the CI Dockerfile):
#   Distro package: `openssl` (Arch) / `libssl-dev` (Debian, Ubuntu). See
#   CLAUDE.md's Dependencies lines, .github/workflows/ci.yml,
#   .github/workflows/release.yml, .github/workflows/codeql.yml (fix
#   round 3 minor C) and .github/docker/Dockerfile, all updated alongside
#   this block.
#
# Windows, MSVC (release.yml's release-artifact leg only):
#   vcpkg, triplet x64-windows-static-md, pinned by a manifest baseline
#   (vcpkg.json at the repo root). Fix round 2: builtin-baseline is
#   9e593bb18ea69cc5095e012465dcd675a822ed0d, the commit vcpkg's annotated
#   release tag 2026.07.29 (published 2026-07-31, tag object
#   c76c06644034521fb761a39f8f52d8e87d1103d5) dereferences to --
#   deliberately a release tag rather than master HEAD, because
#   reproducibility is the whole point of pinning a baseline at all, and a
#   future OpenSSL security update should be a visible one-line bump to a
#   newer tag, not silent drift. Confirmed ports/openssl/vcpkg.json exists
#   at that tree before pinning it, so the dependency actually resolves.
#   Bump by replacing this SHA (and the identical one in vcpkg.json) with
#   a newer release tag's commit when taking an update; do not point this
#   at master HEAD instead.
#
#   Coordinator decision, fix round 1, for why THIS route on THIS leg:
#     - Static (not dynamic) because this project ships a portable ZIP and
#       an NSIS installer, and a missing runtime DLL has already cost a
#       release (v0.5.0 shipped without rade.dll). -static-md
#       specifically, not plain -static: static OpenSSL libs against the
#       DYNAMIC CRT, matching what Qt itself links against on this leg.
#       Mixing CRTs (static OpenSSL against a static CRT, dynamic Qt
#       against a dynamic CRT) is its own class of bug.
#     - vcpkg because `windows-latest` already ships it
#       (VCPKG_INSTALLATION_ROOT is set on the image), so this adds no new
#       tool alongside release.yml's existing `choco install ninja nsis`.
#     - A pinned baseline (not vcpkg's rolling default) because this is a
#       TLS library: vendoring a prebuilt binary blob the way
#       third_party/fftw3/ does for FFTW would mean owning patch
#       distribution by hand when OpenSSL issues an advisory. A baseline
#       makes taking that update a one-line bump with a visible diff
#       instead.
#
#   release.yml passes -DCMAKE_TOOLCHAIN_FILE, -DVCPKG_TARGET_TRIPLET and
#   -DOPENSSL_ROOT_DIR explicitly on the initial `cmake -B build` line (a
#   toolchain file must be supplied before project() runs, so it cannot be
#   set from inside this file). Fix round 3 correction to the assertion
#   below's own original reasoning: CMake's FindOpenSSL sets NO_DEFAULT_PATH
#   once OPENSSL_ROOT_DIR is defined, so the Strawberry Perl collision this
#   assertion was written to catch cannot actually happen on this leg --
#   confirmed by the round 3 reviewer, who ran it. The assertion is kept
#   anyway: it is redundant against a fresh configure but still catches a
#   STALE CMake cache (e.g. a persisted build/ directory reconfigured with
#   a different OPENSSL_ROOT_DIR than the one that populated its cache),
#   which NO_DEFAULT_PATH alone does not guard against.
#
# Windows, MinGW (ci.yml's compile-only smoke-check leg only):
#   Fix round 3, Important A: this leg's Qt is `arch: win64_mingw`, GCC
#   13.1.0 (tools_mingw1310), the classic MSVCRT-based MinGW build, not
#   UCRT. Fix round 5 correction: this sentence originally (fix round 3)
#   supported that claim by asserting "Qt's separate win64_mingw_ucrt
#   architecture exists" -- that name does not exist and was never
#   checked before being written down here, an exact copy of the same
#   fabrication `ci.yml`'s equivalent comment already retracted fix round
#   4 (the two blocks were written together fix round 3, and only one
#   was corrected). Verified (aqtinstall's own docs, fix round 4): Qt's
#   Windows desktop architectures are win64_mingw, win64_llvm_mingw,
#   win64_msvc2022_64 and win64_msvc2022_arm64_cross_compiled -- no
#   `_ucrt` suffix anywhere. mingw-builds (this leg's toolchain) is
#   MSVCRT-based; win64_llvm_mingw (a different, LLVM-based toolchain) is
#   Qt's actual UCRT option. MSYS2's own docs are still the real,
#   separately-verified basis for treating MSVCRT/UCRT as unsafe to mix.
#   A prior revision of this route tried a Qt-distributed OpenSSL tool by
#   the wrong name entirely and, worse, even correctly named it would have
#   been an MSVC-built prebuild -- the wrong ABI for a MinGW target no
#   matter what it was called. Replaced with MSYS2's MINGW64 environment
#   (not UCRT64), package mingw-w64-x86_64-openssl, matching
#   win64_mingw's own MSVCRT runtime -- installed via the msys2/setup-msys2
#   action in ci.yml, landing at `<msys2-location>/mingw64/`. The header
#   and link-library assertions ci.yml already runs against the resolved
#   path (added fix round 2, kept as-is fix round 3 -- they were judged
#   correct) still apply: this route can still hand a runtime-only
#   package the same way the removed one did if MSYS2's package layout
#   ever changes, so they keep failing the job loudly rather than
#   trusting presence alone.
#
# Design note on why static linking is safe despite Qt's own Windows TLS
# story (MSVC leg): Qt on Windows commonly uses Schannel, and where it
# uses OpenSSL instead it may carry its own copy -- ordinarily, two
# OpenSSL runtimes alive in one process is a real hazard (symbol
# collisions, mismatched struct layouts across versions). It is safe here
# specifically BECAUSE this class hands Qt PEM *text*, never an OpenSSL
# object: CertificateStore generates with libcrypto, serialises to PEM,
# writes it to disk, and Qt's QSslCertificate/QSslKey parse that PEM
# independently through whichever backend Qt itself is using. No
# EVP_PKEY*, X509*, or other OpenSSL pointer ever crosses the boundary
# into Qt, so there is no shared state for two OpenSSL runtimes to
# corrupt. Keep it that way -- an optimisation that skips the PEM
# round-trip and hands Qt an OpenSSL object directly would silently
# reintroduce this hazard.
find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)
message(STATUS "OpenSSL ${OPENSSL_VERSION} found at ${OPENSSL_INCLUDE_DIR}")

# R3 Task 1: direct encrypted media transport. The helper fetches only
# SHA-256-pinned codeload archives, keeps media enabled, disables the unused
# WebSocket implementation, and keeps dependency targets out of installation.
include(cmake/NereusRemoteMedia.cmake)
nereus_add_remote_media_dependency()

# iPhone app Task 14 (R-IOS-08, D37): libsodium 1.0.22 and SPAKE2+EE for the
# pairing code's key exchange, both fetched by SHA-256-pinned archive and
# built as static libraries (cmake/NereusPairing.cmake).
include(cmake/NereusPairing.cmake)
nereus_add_pairing_dependency()

# Fix round 3: broadened from WIN32-only to any platform, since the same
# "assert what was explicitly requested is what was actually found"
# principle now also applies to release.yml's macOS x86_64 cross-compile
# row (Important B) -- a stale build/ directory reconfigured after a
# switch between the arm64 native row and the x86_64 cross-compile row
# could otherwise silently keep the wrong architecture's cached OpenSSL.
if(DEFINED OPENSSL_ROOT_DIR)
    # Only checked when the build was actually told which root to expect
    # (release.yml's vcpkg step on the Windows/MSVC leg, or its x86_64
    # OpenSSL source-build step on the macOS Intel leg, sets this; a bare
    # local configure with no OPENSSL_ROOT_DIR is not asserted against
    # anything here, and relies on whatever FindOpenSSL resolves on its
    # own). Turns a silent wrong-pick (see the design notes above) into a
    # hard configure error -- the same "refuse to configure quietly
    # wrong" principle the Qt6::WebSockets block above applies to a
    # missing mandatory feature, extended here to a mandatory dependency
    # resolving to the WRONG copy.
    #
    # Fix round 4: this assertion previously compared OPENSSL_ROOT_DIR
    # against OPENSSL_INCLUDE_DIR directly through cmake_path(IS_PREFIX),
    # which broke BOTH Windows legs. cmake_path's own documentation is
    # explicit that "/" is the only recognised directory separator; it
    # never treats "\" as one. ci.yml's msys2-location output and
    # release.yml's OPENSSL_ROOT_DIR (mangled to backslashes by Git
    # Bash's MSYS path translation under `shell: bash`) are both NATIVE
    # Windows paths, so OPENSSL_ROOT_DIR arrived as e.g.
    # "D:\a\_temp\msys64/mingw64" while FindOpenSSL had already
    # normalised OPENSSL_INCLUDE_DIR to forward slashes -- IS_PREFIX
    # parsed the backslash root as a single opaque path component with
    # no root-directory, which can never prefix-match a path that has
    # one, and FATAL_ERROR fired on every correctly-resolved Windows
    # configure. Reproduced and fixed locally (this machine is not
    # Windows, but cmake_path's parsing is pure string logic, independent
    # of host OS, so this is meaningfully testable here):
    #
    #   backslash root "D:\a\_temp\msys64/mingw64" vs forward-slash
    #   input "D:/a/_temp/msys64/mingw64/include" -> IS_PREFIX: OFF (bug
    #   reproduced)
    #
    # cmake_path(CONVERT ... TO_CMAKE_PATH_LIST ...), the mechanism first
    # proposed for this fix, was tried and rejected: TO_CMAKE_PATH_LIST
    # treats its input as a native PATH-LIST (semicolon/colon-separated,
    # like $PATH), not a single path's separators, and mangled
    # "D:\a\_temp\msys64/mingw64" into "D;\a\_temp\msys64/mingw64" --
    # actively wrong, not merely ineffective. string(REPLACE "\\" "/" ...)
    # is what actually works, applied to both sides so this holds even if
    # a future FindOpenSSL stops pre-normalising OPENSSL_INCLUDE_DIR:
    #
    #   sanitized backslash root vs forward-slash input -> IS_PREFIX: ON
    #   sanitized already-forward-slash root vs same input -> IS_PREFIX: ON (no regression)
    #   sanitized WRONG root (Strawberry Perl-shaped, with backslashes) -> IS_PREFIX: OFF (negative control still rejects)
    #   POSIX root/input (no backslashes at all, e.g. this machine) -> IS_PREFIX: ON (unaffected)
    #
    # The round-2 reviewer's other point stands and is worth restating:
    # this assertion is redundant against a FRESH configure specifically
    # on the MSVC leg (FindOpenSSL sets NO_DEFAULT_PATH once
    # OPENSSL_ROOT_DIR is defined, so the Strawberry Perl collision it
    # guards against cannot occur there). It is kept, not deleted,
    # because it still catches a STALE CMake cache on any platform (a
    # persisted build/ directory reconfigured with a different
    # OPENSSL_ROOT_DIR than the one that populated its cache) -- see the
    # macOS x86_64-vs-arm64 row-switch case documented above -- and NOW
    # also genuinely fires correctly on Windows instead of unconditionally
    # rejecting it.
    string(REPLACE "\\" "/" _openssl_root_sanitized "${OPENSSL_ROOT_DIR}")
    string(REPLACE "\\" "/" _openssl_include_sanitized "${OPENSSL_INCLUDE_DIR}")
    cmake_path(IS_PREFIX _openssl_root_sanitized "${_openssl_include_sanitized}" NORMALIZE _openssl_under_intended_root)
    if(NOT _openssl_under_intended_root)
        message(FATAL_ERROR
            "find_package(OpenSSL) resolved to ${OPENSSL_INCLUDE_DIR}, which is "
            "NOT under the OPENSSL_ROOT_DIR (${OPENSSL_ROOT_DIR}) this build was "
            "explicitly told to use. On Windows/MSVC the likely cause is "
            "Strawberry Perl's bundled MinGW-built OpenSSL, which "
            "windows-latest preinstalls and which is not link-compatible with "
            "an MSVC build; confirm OPENSSL_ROOT_DIR points at the vcpkg "
            "x64-windows-static-md install. On macOS the likely cause is a "
            "stale build/ directory that cached a different architecture's "
            "OpenSSL before switching between the native and cross-compile "
            "rows; delete build/CMakeCache.txt and reconfigure. Either way: "
            "confirm no earlier search path entry lets FindOpenSSL prefer a "
            "different copy.")
    endif()
    unset(_openssl_root_sanitized)
    unset(_openssl_include_sanitized)
    unset(_openssl_under_intended_root)
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
# Dependency archive pins (URL and SHA-256) and the optional directory of
# pre-fetched, hash-checked copies CI provides (NEREUS_DEPENDENCY_ARCHIVE_DIR).
# Used by the FFTW fallback below, the Opus build in third_party/rade, and
# cmake/NereusPairing.cmake and cmake/NereusRemoteMedia.cmake.
# --------------------------------------------------------------------------
include(cmake/NereusDependencyArchives.cmake)

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
        # The pre-fetched copy when there is one, else fftw.org; either
        # way the zip must match the pinned SHA-256.
        nereus_dependency_archive(fftw_win64 _fftw_url _fftw_hash)
        if(_fftw_url MATCHES "^/")
            set(_fftw_url "file://${_fftw_url}")
        elseif(NOT _fftw_url MATCHES "^[a-z]+://")
            # A drive path (D:/...): file(DOWNLOAD) takes it as file:///D:/...
            set(_fftw_url "file:///${_fftw_url}")
        endif()
        set(_fftw_zip "${CMAKE_BINARY_DIR}/fftw-3.3.5-dll64.zip")
        set(_fftw_tmp "${CMAKE_BINARY_DIR}/fftw-tmp")
        message(STATUS "FFTW3: libfftw3-3.dll missing, fetching ${_fftw_url}")
        file(DOWNLOAD "${_fftw_url}" "${_fftw_zip}"
             EXPECTED_HASH ${_fftw_hash}
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
        # pkg-config names the library bare ("fftw3"), which reaches the
        # link line as -lfftw3 twice: once for NereusSDR's own targets and
        # again after the static libraries that also need it (wdsp_static).
        # Apple's ld warns "ignoring duplicate libraries: '-lfftw3'" on
        # that. Linking the full path, as fftw3f already is, keeps the
        # ordering and drops the warning.
        #
        # Searched like fftw3f: the default paths (CMAKE_PREFIX_PATH, then
        # Homebrew's own lib folder) before pkg-config's folder. Homebrew's
        # pkg-config names the versioned Cellar folder, and a cached Cellar
        # path breaks linking in an existing build folder once
        # `brew upgrade fftw` and `brew cleanup` remove it; the unversioned
        # ${HOMEBREW_PREFIX}/lib link follows upgrades. (The cache variable
        # was FFTW3_FULL_PATH; the new name makes existing build folders
        # search again instead of keeping a cached Cellar path.)
        if(FFTW3_FOUND)
            set(_fftw3_hints "")
            if(HOMEBREW_PREFIX)
                list(APPEND _fftw3_hints "${HOMEBREW_PREFIX}/lib")
            endif()
            list(APPEND _fftw3_hints ${FFTW3_LIBRARY_DIRS})
            find_library(FFTW3_LINK_LIBRARY fftw3 HINTS ${_fftw3_hints})
            unset(_fftw3_hints)
            if(FFTW3_LINK_LIBRARY)
                set(FFTW3_LIBRARIES ${FFTW3_LINK_LIBRARY})
            endif()
        endif()
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
        # R-R3-39: WDSP's FFT planning runs on more than one thread (the
        # receive DSP lane and the transmit side), so WdspEngine makes
        # FFTW's planner thread-safe (fftw_make_planner_thread_safe), which
        # lives in libfftw3_threads here. The operator chose FFTW's
        # thread-safe planner (2026-09-24). Found beside libfftw3; Homebrew's
        # fftw and Debian/Ubuntu's libfftw3-dev (runtime: libfftw3-double3)
        # both ship it. Windows: the fftw.org 3.3.5 DLL has the threads
        # functions built into libfftw3-3.dll itself.
        set(_fftw3_threads_hints ${FFTW3_LIBRARY_DIRS})
        if(HOMEBREW_PREFIX)
            list(APPEND _fftw3_threads_hints "${HOMEBREW_PREFIX}/lib")
        endif()
        if(FFTW3_LINK_LIBRARY)
            get_filename_component(_fftw3_dir "${FFTW3_LINK_LIBRARY}" DIRECTORY)
            list(PREPEND _fftw3_threads_hints "${_fftw3_dir}")
            unset(_fftw3_dir)
        endif()
        find_library(FFTW3_THREADS_LIB fftw3_threads HINTS ${_fftw3_threads_hints})
        unset(_fftw3_threads_hints)
        if(FFTW3_THREADS_LIB)
            # Ahead of libfftw3, which it depends on (matters for static
            # archives only).
            list(PREPEND FFTW3_LIBRARIES ${FFTW3_THREADS_LIB})
            message(STATUS "FFTW3 threads found — thread-safe FFT planner: ${FFTW3_THREADS_LIB}")
        else()
            message(FATAL_ERROR
                "libfftw3_threads not found beside libfftw3. WDSP plans FFTs on "
                "more than one thread and needs FFTW's thread-safe planner. "
                "Install it with FFTW (Homebrew: fftw; Debian/Ubuntu: libfftw3-dev) "
                "or build FFTW with --enable-threads.")
        endif()
        # R-R3-39: the thread-safe switch is per precision library. The
        # display FFTs plan with single-precision FFTW on every spectrum
        # thread (FFTEngine::replanFft, WidebandFftEngine), so
        # fftwf_make_planner_thread_safe() is needed too, and it lives in
        # libfftw3f_threads. Found beside libfftw3f; Homebrew's fftw and
        # Debian/Ubuntu's libfftw3-dev (runtime: libfftw3-single3) ship it.
        # Windows: the fftw.org 3.3.5 libfftw3f-3.dll has it built in.
        if(FFTW3F_LIB)
            get_filename_component(_fftw3f_dir "${FFTW3F_LIB}" DIRECTORY)
            find_library(FFTW3F_THREADS_LIB fftw3f_threads
                HINTS "${_fftw3f_dir}" ${FFTW3_LIBRARY_DIRS})
            unset(_fftw3f_dir)
            if(FFTW3F_THREADS_LIB)
                # Ahead of libfftw3f, which it depends on (matters for
                # static archives only).
                list(FIND FFTW3_LIBRARIES "${FFTW3F_LIB}" _fftw3f_pos)
                list(INSERT FFTW3_LIBRARIES ${_fftw3f_pos} ${FFTW3F_THREADS_LIB})
                unset(_fftw3f_pos)
                message(STATUS "FFTW3F threads found: thread-safe float FFT planner: ${FFTW3F_THREADS_LIB}")
            else()
                message(FATAL_ERROR
                    "libfftw3f_threads not found beside libfftw3f. The display FFTs "
                    "plan in single precision on more than one thread and need FFTW's "
                    "thread-safe planner. Install it with FFTW (Homebrew: fftw; "
                    "Debian/Ubuntu: libfftw3-dev) or build FFTW with --enable-float "
                    "--enable-threads.")
            endif()
        endif()
    endif()
    # R-R3-39: FftwPlanner.cpp calls fftw_make_planner_thread_safe() and
    # fftwf_make_planner_thread_safe(), so it must see the fftw3.h that
    # belongs to the FFTW library being linked.
    # A plain <fftw3.h> is not enough: the WDSP include folder
    # (third_party/wdsp/src, public on wdsp_static) carries WDSP's pinned
    # copy of an older fftw3.h without that declaration, and on Linux the
    # system header sits in /usr/include, an implicit folder CMake never
    # passes as -I, so WDSP's copy is searched first and shadows it. Name
    # the platform header by its full path instead: Debian/Ubuntu's
    # /usr/include/fftw3.h, Homebrew's, or third_party/fftw3/include on
    # Windows (the fftw.org 3.3.5 header, which declares it).
    set(NEREUS_FFTW3_HEADER "")
    foreach(_fftw3_inc_dir IN LISTS FFTW3_INCLUDE_DIRS FFTW3_INCLUDEDIR)
        if(NOT NEREUS_FFTW3_HEADER AND EXISTS "${_fftw3_inc_dir}/fftw3.h")
            set(NEREUS_FFTW3_HEADER "${_fftw3_inc_dir}/fftw3.h")
        endif()
    endforeach()
    unset(_fftw3_inc_dir)
    if(NOT NEREUS_FFTW3_HEADER)
        find_path(NEREUS_FFTW3_INCLUDE_DIR fftw3.h)
        if(NEREUS_FFTW3_INCLUDE_DIR)
            set(NEREUS_FFTW3_HEADER "${NEREUS_FFTW3_INCLUDE_DIR}/fftw3.h")
        endif()
    endif()
    if(NOT NEREUS_FFTW3_HEADER)
        message(FATAL_ERROR
            "FFTW3 was found but its fftw3.h was not. FftwPlanner.cpp needs the "
            "header that matches the FFTW library for the thread-safe planner calls.")
    endif()
    message(STATUS "FFTW3 header for the thread-safe planner: ${NEREUS_FFTW3_HEADER}")
    # Only FftwPlanner.cpp (NereusCore, NereusCore_LTO) includes it this way.
    set_property(SOURCE ${CMAKE_SOURCE_DIR}/src/core/FftwPlanner.cpp
        APPEND PROPERTY COMPILE_DEFINITIONS
        "NEREUS_FFTW3_HEADER=\"${NEREUS_FFTW3_HEADER}\"")
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
    src/core/settings/SettingsBackup.cpp
    src/core/settings/SettingsBackupTransfer.cpp
    src/core/BoardCapabilities.cpp
    src/core/BuildIdentity.cpp
    src/core/FFTRouter.cpp
    src/core/SliceStreamAllocator.cpp
    # iPhone app Task 73 (R-IOS-02): whose each slice is, the held-for mark
    # and each owner's active slice.
    src/core/SliceOwnership.cpp
    # Slice control plan Task 2: who may see, hear and change each slice.
    src/core/session/SliceAccessPolicy.cpp
    # iPhone app Task 75 (R-IOS-30): which other devices a radio-wide
    # setting disturbs.
    src/core/DisturbanceCheck.cpp
    src/core/WidebandFrameAccumulator.cpp
    src/core/WidebandFftEngine.cpp
    src/core/spectrum/WidebandDisplayReference.cpp
    src/core/spectrum/ExtendedSpectrumReducer.cpp
    src/core/spectrum/WidebandSpectrumCache.cpp
    src/core/AudioDeviceConfig.cpp
    src/core/SampleRateCatalog.cpp
    # R-R3-49 / R-IOS-18: Radio Info's text, for the tab and the Setup description.
    src/core/RadioInfoFacts.cpp
    # R-R3-49: Calibration's Volts/Amps log (Thetis console.cs LogVA).
    src/core/VoltsAmpsLog.cpp
    src/core/SkuUiProfile.cpp
    src/core/HardwareProfile.cpp
    src/core/LogCategories.cpp
    # Remote-window parity Task 22 (R-R3-49): the log sink no logging
    # thread waits on, and the zip archive the support bundle travels in.
    src/core/LogSink.cpp
    src/core/ZipArchive.cpp
    src/core/SupportBundle.cpp
    src/core/RadioDiscovery.cpp
    src/core/RadioConnection.cpp
    src/core/RadioLinkStats.cpp
    src/core/TciProtocol.cpp
    src/core/TciSendQueue.cpp
    src/core/TciVfoCoalescer.cpp
    src/core/TciUpdateGap.cpp
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
    # R-R3-39: FFTW's planners made thread-safe, double and single precision.
    src/core/FftwPlanner.cpp
    src/core/WdspThreadCheck.cpp
    src/core/DspControlThread.cpp
    src/core/RxChannel.cpp
    src/core/TxChannel.cpp
    src/core/RadeChannel.cpp
    src/core/RadeRxWorker.cpp
    src/core/RadeText.cpp
    # RADE end-of-over callsigns: FreeDV's callsign format (freedv-backend
    # rade_text + LDPC(112,56) port).
    src/core/RadeTextCodec.cpp
    src/core/RadeLdpc.cpp
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
    src/core/dsp/NnrSettings.h
    src/core/dsp/NnrAdapter.cpp
    src/core/dsp/NnrLoadGovernor.cpp
    src/core/dsp/NnrLoadGovernor.h
    src/core/dsp/Ps3Snapshot.h
    src/core/dsp/Ps3DisplayAdapter.cpp
    src/core/dsp/DspAssetValidation.cpp
    src/core/dsp/DspAssetService.cpp
    src/core/dsp/DspAssetService.h
    src/core/session/PureSignalSessionFacade.cpp
    src/core/session/PureSignalSessionFacade.h
    src/core/session/Ps3DisplayCodec.cpp
    src/core/session/Ps3DisplayCodec.h
    src/core/dsp/DspAssetStore.cpp
    src/core/MoxController.cpp
    src/core/MicProfileManager.cpp
    src/core/ParaEqEnvelope.cpp
    src/core/CfcProfile.cpp
    src/core/ParaEqCurve.cpp
    src/core/ParaEqCurve.h
    # Phone wire batch: the Diversity dialog's sensitivity pattern, drawn by
    # DiversityRadarWidget and sent on each slice.
    src/core/DiversityPattern.cpp
    src/core/DiversityPattern.h
    src/core/TwoToneController.cpp
    src/core/TxSliceArbiter.cpp
    src/core/TxSliceArbiter.h
    src/core/FFTEngine.cpp
    src/core/TxAnalyzer.cpp
    src/core/TxDisplayFeed.cpp
    src/core/TxDisplayFeed.h
    src/core/AmModulationAnalyzer.cpp
    src/core/NoiseFloorEstimator.cpp
    src/core/NoiseFloorTracker.cpp
    src/core/ClarityController.cpp
    src/core/StepAttenuatorController.cpp
    src/core/StepAttenuatorFacade.cpp
    src/core/LevelCalibrationRun.cpp
    src/core/LevelCalibrationService.cpp
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
    src/core/IoBoardHl2Facade.cpp
    src/core/HermesLiteBandwidthMonitor.cpp
    src/core/ModelPaths.cpp
    src/core/accessories/AlexController.cpp
    src/core/accessories/AlexAntennaFacade.cpp
    src/core/accessories/ApolloController.cpp
    src/core/accessories/N2adrPreset.cpp
    src/core/accessories/PennyLaneController.cpp
    src/core/audio/MasterMixer.cpp
    src/core/audio/VaxChannelMixer.cpp
    src/core/audio/SpeakerFormatConverter.cpp
    src/core/audio/PcMicSource.cpp
    src/core/audio/RadeTxFilters.cpp
    src/core/audio/RadeTxFilters.h
    src/core/audio/RadioMicSource.cpp
    src/core/audio/VaxTxMicSource.cpp
    src/core/audio/CaptureProtocol.cpp
    src/core/audio/CaptureHelper.cpp
    src/core/audio/CaptureAudioBus.cpp
    src/core/audio/CaptureSupervisor.cpp
    src/core/audio/CaptureHelperLocator.cpp
    src/core/StationBinaryLocator.cpp
    src/core/audio/CompositeTxMicRouter.cpp
    src/core/audio/PortAudioBus.cpp
    src/core/audio/QAudioSinkAdapter.cpp
    src/core/audio/RealtimeAudioPriority.cpp
    src/core/platform/ThreadPlacement.cpp
    src/core/audio/TxMicSource.cpp
    src/core/audio/VirtualCableDetector.cpp
    src/core/safety/BandPlanGuard.cpp
    src/core/safety/SwrProtectionController.cpp
    src/core/safety/TxInhibitMonitor.cpp
    src/core/safety/TxRefusal.cpp
    src/core/safety/StationTxGate.cpp
    src/core/safety/UnkeyGate.cpp
    src/core/safety/TxTimeOutTimer.cpp
    src/core/safety/TransmitHolder.cpp
    src/core/safety/RemoteTxWatchdog.cpp
    src/core/safety/StarvationPolicy.cpp
    src/core/safety/safety_constants.cpp
    src/core/SpotCollectorClient.cpp
    src/core/PotaClient.cpp
    src/core/DxClusterClient.cpp
    src/core/WsjtxClient.cpp
    src/core/FreeDVReporterClient.cpp
    src/core/FreeDVRadeReporterBridge.cpp
    src/core/PskReporterClient.cpp
    # Parity Task 19 (R-IOS-25): starts and stops the spot sources, locally
    # and on the Core, and is the mirrored `spotSources` object.
    src/core/SpotSourceHost.cpp
    src/core/CtyDatParser.cpp
    src/core/AdifParser.cpp
    src/core/DxccWorkedStatus.cpp
    src/core/DxccColorProvider.cpp
    # Phase 3P-II PGXL/TGXL accessories
    src/core/PgxlConnection.cpp
    src/core/PgxlStatusGauges.cpp
    src/core/TgxlConnection.cpp
    src/core/StationTgxlController.cpp
    src/core/StationPgxlController.cpp
    # Phase 3P-III RF-Kit RF2K-S amplifier
    src/core/Rf2ksConnection.cpp
    # R-R3-47 / R-R3-48: the Core's RF-Kit and station TCI server
    src/core/StationRfKitController.cpp
    src/core/StationTciController.cpp
    src/core/StationNetwork.cpp
    src/core/RfKitBandFollow.cpp
    src/core/TciSwitch.cpp
    src/core/LanDiscovery.cpp
    src/core/SmartSdrApiListener.cpp
    src/core/ConnectionDiagnostics.cpp
    # Phase 3P-II TGXL tuner model
    src/models/TunerModel.cpp
    # R-R3-47: the Core's Power Genius and RF-Kit status objects
    src/models/AmplifierModel.cpp
    src/models/RfKitModel.cpp
    src/models/StationTciModel.cpp
    # Phase 3P-II Phase 4 (Advanced UI + helpers)
    src/core/FaultLog.cpp
    src/core/TuneMemoryStore.cpp
    src/core/TxInterlockPolicy.cpp
    # R-R3-47 / R-R3-22: the Core's accessory records and settings
    src/core/StationAccessoryData.cpp
    src/models/AccessoryDataModel.cpp
    # R-R3-47 / R-R3-22: the amp's and tuner's own settings for a window
    src/core/StationDeviceSettings.cpp
    src/models/AccessorySettingsModel.cpp
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
    # iPhone app Task 19 (R-IOS-06): the waterfall palettes, moved from
    # SpectrumWidget so the Core's catalogue can send them to an app.
    src/core/spectrum/WaterfallPalettes.cpp
    # iPhone app Task 20 (R-IOS-27): the display computations the desktop's
    # SpectrumWidget runs, moved here so the Core runs the same code for an
    # app's display: peak blobs, active peak hold, and DisplayFollowers
    # (noise floor, waterfall levels, normalise, averaging constant).
    src/core/spectrum/ActivePeakHoldTrace.cpp
    src/core/spectrum/PeakBlobDetector.cpp
    src/core/spectrum/DisplayFollowers.cpp
    # R1 Task 6: per-stream FFTEngine pool extracted from
    # MainWindow::createFftEngineForStream / m_fftEngines / m_fftThread.
    # Owns engine lifecycle, the four global display AppSettings-sourced
    # knobs (as FftPoolConfig), and the shared worker thread(s), so the
    # daemon can produce spectrum with no widget toolkit in the process.
    src/core/spectrum/FftEnginePool.cpp
    # R3 Task 2: daemon-owned source that feeds the existing FFT pool from
    # live stream I/Q without any GUI or endpoint policy.
    src/core/daemon/DaemonAgcSource.cpp
    src/core/session/media/DaemonSpectrumSource.cpp
    # R3 daemon session owner for media peer, spectrum endpoints, and the
    # existing mixed-audio Opus path.
    src/core/session/media/DaemonMediaController.cpp
    src/core/daemon/DaemonTelemetryController.cpp
    # R-R3-32/33: Linux procfs/sysfs host load for the Core telemetry.
    src/core/daemon/HostTelemetrySampler.cpp
    # R-R3-40/41: the load readings the display load governor may act on.
    src/core/daemon/DisplayLoadInputs.cpp
    # R3 bounded bridge from the AudioEngine master mix to daemon media.
    src/core/session/media/DaemonAudioSource.cpp
    src/core/session/media/DaemonAudioSender.cpp
    # R-R3-23: the audio-context wire codec Core and GUI share (minor 7 and 8).
    src/core/session/media/RemoteAudioContext.cpp
    src/core/session/media/RemoteIqCodec.cpp
    # R-R3-01/08: the spectrum-context wire codec Core and GUI share (minor 8 and 9).
    src/core/session/media/RemoteSpectrumContext.cpp
    src/core/session/media/AudioJitterBuffer.cpp
    src/core/session/media/RemoteAudioRateMatcher.cpp
    # iPhone app plan Task 36 (R-IOS-13): the microphone uplink at the Core
    # (receiver, transmit jitter buffer, rate matching to the radio's clock).
    src/core/session/media/RemoteMicReceiver.cpp
    # R-R3-23 Task 2: RFC 3550 jitter/sequence measurement for one receiver context.
    src/core/session/media/RtpReceptionStats.cpp
    src/core/session/media/RemoteAudioReceiver.cpp
    # R-R3-44: a remote window's VAX channel fed by a receiver stream, paced
    # by the VAX output's own clock.
    src/core/audio/RemoteVaxFeeder.cpp
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
    src/core/station/StationHost.cpp
    src/core/station/StationSliceOwnershipPolicy.cpp
    src/core/station/StationHandover.cpp
    # iPhone app Task 17 (R-IOS-08): the Core's read-only status page, its
    # owner-only console socket and the console commands it answers.
    src/core/daemon/StationStatusPage.cpp
    src/core/daemon/StationControlSocket.cpp
    src/core/daemon/StationControlCommands.cpp
    # R-R3-34: bounded per-radio receive-layout persistence. Runtime
    # restoration is wired separately at the RadioModel/daemon boundary.
    src/core/ReceiveLayoutStore.cpp
    # iPhone app Task 73 (R-IOS-02): each device's closed slices, with a copy
    # of their own settings, kept per device and per radio for its return.
    src/core/DeviceLayoutStore.cpp
    # R2 Task 7: the state-mirroring core. MirrorSchema walks Qt's
    # meta-object system over the five mirrored model classes and gives
    # each Q_PROPERTY a dense session-scoped ordinal plus a wire kind;
    # MirrorPolicy is the default-deny direction table saying which of
    # those a remote GUI may write back; StateMirror is the outbound
    # forwarder that watches instances with one zero-argument slot per
    # object and re-reads dirty properties rather than taking signal
    # arguments. Core-side by construction: they reach the models only
    # through QMetaObject, so nothing here includes a GUI header. No
    # production caller yet -- Task 18 binds them onto a wss session.
    src/core/session/MirrorSchema.cpp
    src/core/session/MirrorEnumDomain.cpp
    src/core/session/MirrorPolicy.cpp
    src/core/session/StateMirror.cpp
    # iPhone app Task 72 (R-IOS-02): one device's view of the mirror, with
    # its own coalescer, burst and sink; echo withheld from the writer only.
    src/core/session/MirrorView.cpp
    # iPhone app Task 73 (R-IOS-02): the read-only marker another device sees
    # for a slice that is not its own, one per slice.
    src/core/session/SliceMarker.cpp
    # Slice control and shared listening plan Task 4: who controls and who
    # listens to each slice (SliceAccess, access:<id>), and the listen,
    # stop listening, take control and release checks in one place.
    src/core/session/SliceAccessSet.cpp
    src/core/session/SliceAccessController.cpp
    # iPhone app plan Task 78 (R-IOS-02, R-IOS-30): a remote window's copy of
    # who else is on the Core, their slices, and the Core's questions and
    # notices for it.
    src/core/session/RemoteDevicesState.cpp
    # Slice control and shared listening plan Task 5: a remote window's copy
    # of the access:<id> objects, which marks a listened slice read-only.
    src/core/session/SliceAccessMirror.cpp
    # R2 Task 9: the slice-lifecycle half of the mirror. StateMirror forwards
    # property deltas for objects it already knows about; ObjectRegistry is
    # what tells a remote peer a slice appeared or vanished at all, by
    # watching RadioModel::sliceAdded/sliceRemoved and calling
    # StateMirror::watch()/unwatch() at the right moments. Includes
    # models/RadioModel.h and models/SliceModel.h (unlike the three files
    # above, which stay model-agnostic on purpose) because this class's job
    # is inherently tied to those two signals; TxSliceArbiter.cpp is the
    # existing precedent for src/core/ depending on src/models/ directly.
    src/core/session/ObjectRegistry.cpp
    # R2 Task 10: the wire-message shapes (schema, object.create,
    # object.destroy, delta, the snapshot-complete marker) and their JSON
    # codec. Model-agnostic like MirrorSchema/MirrorPolicy -- it only knows
    # about MirrorUpdate, never a model class -- so it stays out of the
    # ObjectRegistry.cpp block above.
    src/core/session/SessionMessages.cpp
    # iPhone app Task 4 (R-IOS-01): link majors agreed both ways.
    src/core/session/LinkVersion.cpp
    # R3 completion carry, review I1 (R-R3-38, R-IOS-01): the takeover and
    # version reasons, formatted and parsed in one place.
    src/core/session/SessionEndReasons.cpp
    src/core/session/StationTelemetry.cpp
    # R2 Task 11: command verbs. Turns a decoded CommandInvoke SessionMessage
    # into the corresponding RadioModel call (addSlice / removeSlice /
    # requestSliceSampleRate / addSliceOnPan) and reports back what actually
    # happened as a CommandResult. Includes models/RadioModel.h and
    # models/SliceModel.h for the same reason ObjectRegistry.cpp does
    # (comment above) -- this class's job is inherently tied to those
    # concrete entry points, unlike SessionMessages/MirrorSchema/
    # MirrorPolicy, which stay model-agnostic. No production caller yet --
    # Task 18 feeds a real wss session's inbound bytes into dispatch().
    src/core/session/SessionCommandDispatcher.cpp
    # R2 Task 12: the per-slice S-meter gets a model home. Extracted from
    # src/gui/meters/MeterPoller.cpp's pollSliceSMeters()/pollSMeter() so a
    # headless nereusd -- which never links MeterPoller, a GUI class --
    # also produces SliceModel::signalStrengthDbm for the mirror to carry.
    # Owned directly by RadioModel (constructed only for Role::Local); see
    # TxSliceArbiter.cpp above for the existing precedent of src/core/
    # depending on src/models/ directly.
    src/core/meters/SliceMeterPump.cpp
    # iPhone app plan Task 39 (D14, R-IOS-13): the Core's transmit meters,
    # ten times a second while keyed, for the `txState` object.
    src/core/meters/TxMeterPump.cpp
    # R2 Task 14: classifySettingsKey, the pure function deciding whether
    # an AppSettings key belongs to the station (must round-trip over the
    # wire) or to the operator's own machine (must never leave the local
    # store). Task 15's SettingsProxy is the only planned caller so far.
    # No I/O, no singleton, no dependency on AppSettings itself -- see the
    # file's own header comment for the ordered rule table and
    # tests/tst_settings_scope.cpp for the completeness sweep that
    # extracts every key literal this tree passes to an AppSettings
    # accessor and cross-checks this table against it.
    src/core/settings/SettingsScope.cpp
    # R2 Task 15: the AppSettings delegation seam's two live components.
    # SettingsProxy.cpp is the client-side cache installed via
    # AppSettings::setRemoteBackend() on a remote-mode GUI. SettingsProxyServer.cpp
    # is the daemon-side wrapper: builds the connect-time snapshot and
    # applies inbound client writes onto the daemon's own AppSettings with
    # the anti-echo suppression AppSettings.h:187-198's contract requires
    # (see that class's own header comment for why the guard lives here,
    # not inside AppSettings). ISettingsBackend.h (the interface the first
    # of these implements) is header-only, nothing to add here.
    src/core/settings/SettingsProxy.cpp
    src/core/settings/SettingsProxyServer.cpp
    # R2 Task 18: the wss session itself, both halves. StationServer is the
    # daemon side (QWebSocketServer + TLS from Task 17 + TokenStore, driving
    # StateMirror / ObjectRegistry / SessionCommandDispatcher /
    # SettingsProxyServer); StationClient is the GUI side (applies the
    # capability descriptor onto a Role::Remote RadioModel, builds the
    # client-side mirror, feeds SettingsProxy). SessionTransport is the seam
    # between the protocol and whatever carries it, which is what lets the
    # handshake and ordering assertions run without TLS and lets a silently
    # dead peer be simulated at all. StationCapabilities is the descriptor
    # struct plus its codec, included by RadioModel.h for the production
    # handshake entry point.
    src/core/session/SessionTransport.cpp
    src/core/session/StationCapabilities.cpp
    src/core/session/SettingsHygieneWire.cpp
    src/core/session/StationServer.cpp
    src/core/session/TxWatchServer.cpp
    src/core/session/StationOpeningGate.cpp
    src/core/session/StationReceivers.cpp
    src/core/session/StationSharedSettings.cpp
    src/core/session/StationTransmitTake.cpp
    src/core/session/ReceiverPlanner.cpp
    src/core/session/ConfirmStep.cpp
    src/core/session/StationLanAnnouncement.cpp
    src/core/session/StationLanCache.cpp
    src/core/session/StationLanDiscovery.cpp
    src/core/session/StationLanAnnouncer.cpp
    src/core/session/CoreAddresses.cpp
    src/core/session/IceDiagnostics.cpp
    # iPhone app Task 16 (D36): Bonjour. The platform backend is added by
    # nereus_apply_core_deps().
    src/core/session/DnsSdAdvertiser.cpp
    src/core/session/StationClient.cpp
    src/core/session/TxWatchClient.cpp
    # R3 media transport, signalling, display, spectrum, and Opus building
    # blocks. IMediaTransport carries Q_OBJECT and is listed so AUTOMOC emits
    # its signal metadata.
    src/core/session/media/IMediaTransport.h
    src/core/session/media/LibDataChannelMediaTransport.cpp
    src/core/session/media/MediaPeer.cpp
    src/core/session/media/OpusAudioCodec.cpp
    src/core/session/media/PcmAudioCodec.cpp
    src/core/session/media/DisplayCodec.cpp
    # iPhone app Task 20 (R-IOS-27): display extras and the NSDX datagram.
    src/core/session/media/DisplayExtras.cpp
    src/core/session/media/DisplayBudget.cpp
    src/core/session/media/DisplayBudgetSplit.cpp
    src/core/session/media/DisplayLoadGovernor.cpp
    src/core/session/media/DssWideRow.cpp
    src/core/session/media/SpectrumEndpoint.cpp
    src/core/session/media/WidebandDisplayContext.cpp
    # R2 Task 20: where a GUI process gets its station address and token
    # from. A plain value struct plus one URL rule; deliberately carries no
    # AppSettings key literals (see RemoteStationOptions.h for why).
    src/core/session/RemoteStationOptions.cpp
)

# R2 Task 18: the generated pre-shared token nereusd authenticates clients
# against. Sits beside CertificateStore in src/core/security/ and stores its
# secret the same way (a permission-restricted file beside the daemon
# profile, never an AppSettings key -- see TokenStore.h).
list(APPEND CORE_SOURCES src/core/security/TokenStore.cpp)

# R2 Task 17: TLS certificate provisioning for nereusd's future wss::
# listener (Task 18). Unconditional (fix round 1 review, Important 1):
# find_package(OpenSSL) above is REQUIRED, so this file is always
# available to compile.
list(APPEND CORE_SOURCES src/core/security/CertificateStore.cpp)

# iPhone app Task 12 (R-IOS-08): the Core's own P-256 identity key, its
# label, its paired devices and device sign-in. OpenSSL libcrypto, as
# CertificateStore above.
list(APPEND CORE_SOURCES
    src/core/security/StationIdentity.cpp
    src/core/security/StationLabel.cpp
    src/core/security/DeviceStore.cpp
    src/core/security/DeviceAuthenticator.cpp
    # iPhone app Task 13 (R-IOS-08): the mirrored `devices` object and the
    # device administration verbs behind it.
    src/core/session/StationDevicesFacade.cpp
    # iPhone app Task 19 (R-IOS-06): the mirrored `catalog` object.
    src/core/session/StationCatalog.cpp
    src/core/session/BandLinkFit.cpp
    # R-R3-49 / R-IOS-18: the Core's PA Gain profiles for a phone.
    src/core/session/PaProfilesFacade.cpp
    # iPhone app Task 25 (D41): the unbuilt features list the catalogue's
    # tools read (the desktop's hide helpers stay in src/gui).
    src/core/UnbuiltFeatureList.cpp
    src/core/setup/SetupDescriptionService.cpp
    src/core/setup/SetupDescriptionV15.cpp
    # Parity Task 19 (R-IOS-25): the link's record streams.
    src/core/session/RecordStream.cpp
    # R-IOS-13 / R-R3-49 (txModMonitorVersion 1): the AM Mod Monitor's
    # readings on two record streams, and the Core's side that feeds them.
    src/core/session/ModMonitorRecord.cpp
    src/core/session/ModMonitorPublisher.cpp
    # Parity Task 21 (R-IOS-18): the Core's radios and its choice of one.
    src/core/station/StationRadios.cpp
    # iPhone app Task 71 (R-IOS-02): who holds a place on the Core, and the
    # mirrored `connectedDevices` object.
    src/core/session/DeviceSessionRegistry.cpp
    src/core/session/ConnectedDevicesFacade.cpp
    # iPhone app plan Task 39 (D14, R-IOS-13, R-IOS-21): the mirrored
    # `txState` object (TransmitState).
    src/core/session/TransmitStateFacade.cpp
    # iPhone app plan Task 25 (R-IOS-18): the mirrored `vax` object.
    src/core/session/StationVaxFacade.cpp
    # iPhone app plan Task 35 (R-IOS-13): keying from a remote device.
    src/core/session/RemoteKeying.cpp
    # iPhone app plan, desktop remote transmit (R-IOS-13, R-R3-42): the
    # window's side of the transmit verbs.
    src/core/session/RemoteTransmitClient.cpp
    # iPhone app Task 14 (R-IOS-08, D37): the pairing window, its code and
    # the SPAKE2+EE exchange (libsodium, cmake/NereusPairing.cmake).
    src/core/security/PairingCode.cpp
    src/core/security/PairingWindow.cpp
    src/core/security/SpakeExchange.cpp
    # iPhone app Task 18 (R-IOS-08): the desktop's own device key and the
    # client side of pairing.
    src/core/security/ClientDeviceIdentity.cpp
    src/core/session/StationPairingClient.cpp
    # iPhone app plan Task 27 (R-IOS-08, R-IOS-16): the remote access
    # service (the rendezvous): its wire, the Core's and the desktop's
    # client of it, pairing through its mailbox, and the ICE settings of a
    # connection it introduced.
    src/core/session/RendezvousWire.cpp
    src/core/session/RendezvousClient.cpp
    src/core/session/RendezvousMailboxTransport.cpp
    src/core/session/StationRendezvous.cpp
    src/core/session/IceConfiguration.cpp
    src/core/session/CandidateSourceLease.cpp
    # iPhone app plan Task 28 (R-IOS-16): the control session over a data
    # channel, for a connection the service introduced.
    src/core/session/DataChannelTransport.cpp
    # Test builds only (the file is empty without NEREUS_BUILD_TESTS).
    src/core/session/DataChannelLibraryLogTestHook.cpp
    src/core/session/RendezvousDialer.cpp
    # iPhone app plan Task 29 (R-IOS-16): moving a session to a better
    # path (the link document, section 21): the switchable transport under
    # each session, the path race and its rungs, and the media replace's
    # duplicate filter.
    src/core/session/SwitchableTransport.cpp
    src/core/session/ControlLog.cpp
    src/core/session/PathRacer.cpp
    src/core/session/RelayLeg.cpp
    src/core/session/MediaTunnel.cpp
    src/core/session/NetworkTrouble.cpp
    src/core/session/SystemProxy.cpp
    src/core/session/media/RtpDuplicateFilter.cpp
    src/core/session/media/DualPathAudio.cpp
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
    src/models/ReceiverDspLoadSampler.cpp
    src/models/Band.cpp
    src/models/BandDefaults.cpp
    src/models/BandPlanManager.cpp
    src/models/PureSignalSettings.cpp
    src/models/PureSignalSettings.h
    src/models/SliceModel.cpp
    src/models/PanadapterModel.cpp
    src/models/MeterModel.cpp
    src/models/TransmitModel.cpp
    src/models/FilterPresetStore.cpp
    src/models/NotchModel.cpp
    src/models/SpotModel.cpp
    src/models/SpotModeResolver.cpp
    src/models/SpotTableModel.cpp
    src/models/BandFilterProxy.cpp
    src/models/FreeDVStationModel.cpp
    src/models/RxDecodeModel.cpp
    src/models/DisplaySettingsModel.cpp
)

set(GUI_SOURCES
    src/gui/MainWindow.cpp
    src/gui/DesktopStationController.cpp
    src/gui/DesktopStationController.h
    src/gui/HostingSliceActions.cpp
    src/gui/HostingSliceActions.h
    src/gui/GuiDesktopStationRuntime.cpp
    src/gui/GuiDesktopStationRuntime.h
    src/gui/SliceFlagPresentationBinding.cpp
    src/gui/SliceFlagPresentationBinding.h
    src/gui/RemoteMediaController.cpp
    # Remote-window parity Task 29 (A11, R-R3-49): one MOX display
    # controller for both windows, and its local and remote sources.
    src/gui/MoxDisplayController.cpp
    src/gui/TxDisplaySource.cpp
    # iPhone app plan, desktop remote transmit (R-IOS-13, R-R3-42): the
    # remote window's TCI transmit forwarder.
    src/gui/RemoteTransmitForwarder.cpp
    # R-R3-44: a remote window's VAX channels, fed by receiver streams.
    src/gui/RemoteVaxRouter.cpp
    # iPhone app plan Task 78 (R-IOS-02, R-IOS-07, R-IOS-30): the remote
    # window's screens for several devices on one Core.
    src/gui/multidevice/DeviceWords.cpp
    src/gui/multidevice/TakeTransmitDialog.cpp
    src/gui/multidevice/ConfirmChangeDialog.cpp
    src/gui/multidevice/TakeReceiverDialog.cpp
    src/gui/multidevice/ReplaceDeviceDialog.cpp
    src/gui/multidevice/NoticeCard.cpp
    src/gui/multidevice/ConnectedDevicesList.cpp
    src/gui/multidevice/MultiDeviceController.cpp
    src/gui/RemoteAudioStatus.cpp
    src/gui/RemoteDisplayAllocator.cpp
    src/gui/PanStatusText.cpp
    src/gui/OperatorReasonText.cpp
    src/gui/ReceiverStopNotices.cpp
    src/gui/RemoteTelemetryController.cpp
    src/gui/RemoteDiagnosticsDialog.cpp
    src/gui/RemoteConnectionController.cpp
    src/gui/CoreTargetStore.cpp
    src/gui/CoreAddressProbe.cpp
    src/gui/CoreAddressController.cpp
    src/gui/CoreRenameController.cpp
    src/gui/RemoteAudioWidget.cpp
    src/gui/setup/CoreAudioSetupPage.cpp
    src/gui/CoreSettingsHost.cpp
    src/gui/StationStartupSelection.cpp
    src/gui/StationServiceManager.cpp
    src/gui/StationLanSelection.cpp
    src/gui/GuiSessionCoordinator.cpp
    src/gui/GuiConnectionController.cpp
    src/gui/ConnectionSelector.cpp
    src/gui/CoreTargetEditor.cpp
    src/gui/AmpViewChart.cpp
    src/gui/AmpViewWindow.cpp
    src/gui/DspAssetDialog.cpp
    src/gui/DspAssetDialog.h
    src/gui/PsaIndicatorWidget.cpp
    src/gui/PsaIndicatorWidget.h
    src/gui/PsForm.cpp
    src/gui/TitleBar.cpp
    src/gui/RelayBar.cpp
    src/gui/SMeterWidget.cpp
    src/gui/VintageMeterFace.cpp
    src/gui/ConnectionPanel.cpp
    src/gui/AddCustomRadioDialog.cpp
    src/gui/SupportDialog.cpp
    src/gui/AboutDialog.cpp
    src/gui/VaxFirstRunDialog.cpp
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
    src/gui/widgets/NnrControls.cpp
    src/gui/widgets/NnrControls.h
    src/gui/widgets/FilterPassbandWidget.cpp
    src/gui/widgets/FilterPresetEditDialog.cpp
    src/gui/widgets/GradientPickerWidget.h
    src/gui/widgets/GradientPickerWidget.cpp
    src/gui/widgets/GradientStop.h
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
    src/gui/widgets/SystemTile.cpp
    src/gui/widgets/CpuRowCycler.cpp
    src/gui/chrome/ChromeFoldPlan.cpp
    src/gui/chrome/ChromeBarController.cpp
    src/gui/chrome/ChromeBarItems.cpp
    src/gui/widgets/StatusBadge.cpp
    src/gui/widgets/StatusBadge.h
    src/gui/widgets/AdcOverloadBadge.cpp
    src/gui/widgets/AdcOverloadBadge.h
    src/gui/widgets/OverflowChip.cpp
    src/gui/widgets/OverflowChip.h
    src/gui/widgets/RxDashboard.cpp
    src/gui/widgets/RxDashboard.h
    src/gui/SliceChooser.cpp
    src/gui/SliceChooser.h
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
    src/gui/AudioClockEstimator.cpp
    src/gui/AudioClockEstimator.h
    src/gui/RemoteDisplayPresenter.cpp
    src/gui/RemoteDisplayPresenter.h
    src/gui/TelemetryHistory.cpp
    src/gui/TelemetryHistory.h
    src/gui/TimeSeriesGraphWidget.h
    src/gui/DiversityDialog.cpp
    src/gui/DiversityDialog.h
    src/gui/DssRenderer.cpp
    src/gui/SpotHubDialog.cpp
    src/gui/SpotHubDialog.h
    src/gui/FreeDVReporterDialog.cpp
    src/gui/FreeDVReporterDialog.h
    src/gui/LanScanDialog.cpp
    src/gui/LanScanDialog.h
    src/gui/PgxlSaveRebootDialog.cpp
    src/gui/PgxlSaveRebootDialog.h
    src/gui/containers/ContainerTypes.h
    src/gui/containers/ContainerDocument.h
    src/gui/containers/ContainerDocument.cpp
    src/gui/containers/ContainerDocumentCodec.h
    src/gui/containers/ContainerDocumentCodec.cpp
    src/gui/containers/ContainerContentRegistry.h
    src/gui/containers/ContainerContentRegistry.cpp
    src/gui/containers/ContainerArrangeController.h
    src/gui/containers/ContainerArrangeController.cpp
    src/gui/containers/ContainerEditSession.h
    src/gui/containers/ContainerEditSession.cpp
    src/gui/containers/ContainerPreviewWidget.h
    src/gui/containers/ContainerPreviewWidget.cpp
    src/gui/containers/ContainerContentHost.h
    src/gui/containers/ContainerContentHost.cpp
    src/gui/containers/ContainerSourceAdapter.h
    src/gui/containers/ContainerSourceAdapter.cpp
    src/gui/containers/LegacyContainerImporter.h
    src/gui/containers/LegacyContainerImporter.cpp
    src/gui/containers/ContainerWorkspaceStore.h
    src/gui/containers/ContainerWorkspaceStore.cpp
    src/gui/containers/ContainerWidget.cpp
    src/gui/containers/FloatingContainer.cpp
    src/gui/containers/ContainerManager.cpp
    src/gui/containers/ContainerButtonDispatcher.cpp
    src/gui/containers/ContainerSettingsDialog.cpp
    src/gui/containers/ContentPropertyEditor.cpp
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
    src/gui/meters/MeterWidget.cpp
    src/gui/meters/presets/BarPresetItem.cpp
    src/gui/meters/presets/BarPresetItem.h
    src/gui/meters/presets/CompositePresetItem.cpp
    src/gui/meters/presets/CompositePresetItem.h
    src/gui/meters/presets/SMeterPresetItem.cpp
    src/gui/meters/presets/SMeterPresetItem.h
    src/gui/meters/presets/PowerSwrPresetItem.cpp
    src/gui/meters/presets/PowerSwrPresetItem.h
    src/gui/meters/presets/AnanMultiMeterItem.cpp
    src/gui/meters/presets/AnanMultiMeterItem.h
    src/gui/meters/presets/CrossNeedleItem.cpp
    src/gui/meters/presets/CrossNeedleItem.h
    src/gui/meters/presets/MagicEyePresetItem.cpp
    src/gui/meters/presets/MagicEyePresetItem.h
    src/gui/meters/presets/SignalTextPresetItem.cpp
    src/gui/meters/presets/SignalTextPresetItem.h
    src/gui/meters/presets/HistoryGraphPresetItem.cpp
    src/gui/meters/presets/HistoryGraphPresetItem.h
    src/gui/meters/presets/VfoDisplayPresetItem.cpp
    src/gui/meters/presets/VfoDisplayPresetItem.h
    src/gui/meters/presets/ClockPresetItem.cpp
    src/gui/meters/presets/ClockPresetItem.h
    src/gui/meters/presets/ContestPresetItem.cpp
    src/gui/meters/presets/ContestPresetItem.h
    src/gui/meters/presets/PresetGeometry.h
    src/gui/meters/MeterDynamics.cpp
    src/gui/meters/MeterDynamics.h
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
    src/gui/meters/VfoDisplayItem.cpp
    src/gui/meters/ClockItem.cpp
    src/gui/meters/ClickBoxItem.cpp
    src/gui/meters/DataOutItem.cpp
    src/gui/AntennaPopupBuilder.cpp
    src/gui/HGauge.cpp
    src/gui/ColorSwatchButton.cpp
    src/gui/SpectrumOverlayPanel.cpp
    src/gui/SetupDialog.cpp
    src/gui/UnbuiltFeatures.cpp
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
    src/gui/setup/hardware/CalibrationTab.cpp
    src/gui/LevelCalGridFollowGuard.cpp
    src/gui/setup/hardware/PaCalibrationGroup.cpp
    src/gui/setup/hardware/Hl2IoBoardTab.cpp
    src/gui/setup/hardware/Hl2OptionsTab.cpp
    src/gui/setup/hardware/BandwidthMonitorTab.cpp
    src/gui/setup/AudioAdvancedPage.cpp
    src/gui/setup/AudioBackendStrip.cpp
    src/gui/setup/AudioDevicesPage.cpp
    src/gui/setup/AudioTxInputPage.cpp
    src/gui/setup/CaptureStatusText.cpp
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
    src/gui/setup/RemoteStationPage.cpp
    src/gui/setup/PgxlAdvancedPage.cpp
    src/gui/setup/TgxlAdvancedPage.cpp
    src/gui/setup/PgxlInterlockPage.cpp
    src/gui/setup/FourO3APage.cpp
    # Parity Task 21 (R-IOS-18): Setup > This Core in a remote window.
    src/gui/setup/ThisCorePage.cpp
    src/gui/setup/CoresSetupPage.cpp
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
    src/gui/applets/DisplayApplet.cpp
    src/gui/applets/EqApplet.cpp
    src/gui/applets/VaxApplet.cpp
    src/gui/applets/FmApplet.cpp
    src/gui/applets/DigitalApplet.cpp
    src/gui/applets/PureSignalApplet.cpp
    src/gui/applets/ModMonitorApplet.cpp
    src/gui/applets/AppletFloatingWindow.cpp
    src/gui/VintageModMeterWidget.cpp
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

# R-R3-10 / R-R3-21: the band plans belong to the Core, not the GUI.
# BandPlanManager (a NereusCore source, called from RadioModel) reads
# :/bandplans, so the files are compiled into NereusCore. While they sat in
# resources.qrc on NereusSDRLib, any program that used only Core symbols
# (nereusd, and tst_bandplan_manager once GCC's default --as-needed dropped
# the unused GUI library) loaded no band plans at all. The app still sees
# the same five files: it loads NereusCore too. BandPlanManager.cpp also
# calls Q_INIT_RESOURCE(bandplans) so the resource stays reachable from a
# Core-only link of any kind.
# iPhone app Task 14 (R-IOS-08): the pairing code's word list belongs to the
# Core too (PairingCode, which calls Q_INIT_RESOURCE(pairing) likewise). The
# iPhone app carries a byte-for-byte copy of the same file.
qt_add_resources(CORE_RESOURCES resources/bandplans.qrc resources/pairing.qrc resources/setup.qrc)

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
# Shared application libraries, built once and linked dynamically by the
# executable and every test target. NereusCore contains the headless half
# used by nereusd. NereusSDRLib contains the GUI half and links NereusCore.
#
# APP_ICON_SOURCES is intentionally NOT in either shared library: CMake's
# MACOSX_PACKAGE_LOCATION property only takes effect when the file is a
# direct source of a MACOSX_BUNDLE TRUE target. Libraries propagate linkable
# code, not bundle-resource sources. Attach the icon to
# the executable target below so .icns lands in Contents/Resources/ and
# Windows .rc lands on the linker line.
# --------------------------------------------------------------------------
# Split for the headless daemon (docs/architecture/2026-07-28-remote-daemon-architecture-design.md
# section 4.1).  NereusCore must reference no widget symbol so nereusd can link
# it alone. NereusSDRLib keeps PR #314's application-library contract while
# preserving the Core/GUI boundary required by the daemon.
add_library(NereusCore SHARED
    ${CORE_SOURCES}
    ${MODEL_SOURCES}
    ${CORE_RESOURCES}
)

add_library(NereusSDRLib SHARED
    ${GUI_SOURCES}
    ${RESOURCES}
)

# Linux-only sources: the PipeWire/pactl first-run + diagnose dialog uses
# POSIX <unistd.h> (getuid) and the LinuxAudioBackend probe. MainWindow.cpp
# already gates the menu wiring on Q_OS_LINUX, so the .cpp shouldn't enter
# the build on macOS or Windows.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(NereusSDRLib PRIVATE src/gui/VaxLinuxFirstRunDialog.cpp)
endif()

# 2026-05-26 KG4VCF: parallel LTO-enabled object library pair, ONLY built
# when NEREUSSDR_ENABLE_LTO=ON. Same sources as NereusCore/NereusSDRLib above,
# but compiled with LTO bitcode so the main NereusSDR app's link can
# do cross-translation-unit optimization.  Tests continue to link the
# plain shared libraries (no bitcode), so test link times stay fast.
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
        ${CORE_RESOURCES}
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

# Shared-library plumbing. The source tree has no export annotations, so
# Windows builds export all symbols from both application-private DLLs.
foreach(_nereus_shared_target NereusCore NereusSDRLib)
    set_target_properties(${_nereus_shared_target} PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON)
    if(MINGW)
        target_link_options(${_nereus_shared_target} PRIVATE
            "-Wl,--export-all-symbols")
    endif()
endforeach()

# No VERSION/SOVERSION: these libraries are private implementation details.
# Keep transitive shared-library dependencies discoverable beside each other.
if(APPLE)
    foreach(_nereus_shared_target NereusCore NereusSDRLib)
        set_target_properties(${_nereus_shared_target} PROPERTIES
            MACOSX_RPATH ON
            INSTALL_NAME_DIR "@rpath"
            INSTALL_RPATH "@loader_path")
    endforeach()
elseif(UNIX)
    foreach(_nereus_shared_target NereusCore NereusSDRLib)
        set_target_properties(${_nereus_shared_target} PROPERTIES
            INSTALL_RPATH "$ORIGIN")
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
    target_link_libraries(NereusSDR PRIVATE NereusSDRLib)
endif()

if(APPLE)
    set_target_properties(NereusSDR PROPERTIES
        BUILD_RPATH   "@executable_path/../Frameworks"
        INSTALL_RPATH "@executable_path/../Frameworks")
elseif(UNIX)
    set_target_properties(NereusSDR PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib")
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
# key; inject it post-build. The bundle is signed once, after every file
# the build puts in it, by the last POST_BUILD at the end of this file
# (R-R3-50).
#
# Uses Set-then-Add pattern via /bin/sh so the operation is idempotent:
# Set succeeds when the key already exists from a prior build; falls
# through to Add on the first build (when the key is absent).
if(APPLE)
    set(_nereus_mic_desc "NereusSDR uses the microphone for SSB voice transmission and live mic level monitoring.")
    # R-R3-17 / R-R3-38: without NSLocalNetworkUsageDescription macOS Local
    # Network privacy can block connections to radios and Cores on the LAN
    # with no prompt, surfacing only as "Host unreachable".
    set(_nereus_lan_desc "NereusSDR connects to your radios and NereusSDR Core stations on your local network.")
    configure_file(packaging/macos/NereusStation-Info.plist.in
                   "${CMAKE_CURRENT_BINARY_DIR}/generated/NereusStation-Info.plist" @ONLY)
    add_custom_command(TARGET NereusSDR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Frameworks"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:NereusCore>" "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Frameworks/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:NereusSDRLib>" "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Frameworks/"
        COMMAND /bin/sh -c "/usr/libexec/PlistBuddy -c \"Set :NSMicrophoneUsageDescription '${_nereus_mic_desc}'\" \"$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Info.plist\" 2>/dev/null || /usr/libexec/PlistBuddy -c \"Add :NSMicrophoneUsageDescription string '${_nereus_mic_desc}'\" \"$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Info.plist\""
        COMMAND /bin/sh -c "/usr/libexec/PlistBuddy -c \"Set :NSLocalNetworkUsageDescription '${_nereus_lan_desc}'\" \"$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Info.plist\" 2>/dev/null || /usr/libexec/PlistBuddy -c \"Add :NSLocalNetworkUsageDescription string '${_nereus_lan_desc}'\" \"$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Info.plist\""
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Helpers"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:nereus-audio-capture>" "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Helpers/"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Helpers/NereusStation.app/Contents/MacOS"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:nereusd>" "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Helpers/NereusStation.app/Contents/MacOS/nereusd"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/generated/NereusStation-Info.plist" "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Helpers/NereusStation.app/Contents/Info.plist"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Helpers/NereusStation.app/Contents/Resources/models/rnnoise"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/third_party/rnnoise/models/Default_large.bin" "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Helpers/NereusStation.app/Contents/Resources/models/rnnoise/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/third_party/rnnoise/models/Default_small.bin" "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Helpers/NereusStation.app/Contents/Resources/models/rnnoise/"
        COMMENT "Stage private libraries and the capture helper, inject NSMicrophoneUsageDescription and NSLocalNetworkUsageDescription"
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
target_compile_definitions(nereusd PRIVATE NEREUS_DAEMON_EXECUTABLE=1)
target_link_libraries(nereusd PRIVATE NereusCore)
add_dependencies(NereusSDR nereusd)
if(APPLE)
    set_target_properties(nereusd PROPERTIES
        BUILD_RPATH "@executable_path/../../../../Frameworks")
endif()

# --------------------------------------------------------------------------
# nereus-audio-capture -- private microphone helper process (R-R3-36; see
# docs/architecture/2026-09-22-optional-microphone-capture-design.md).
# NereusSDR and nereusd start it over inherited pipes so a native input
# open that never returns cannot stall them. Links NereusCore only, like
# nereusd, and is part of the default build so it is always produced from
# the same revision as its parent.
# --------------------------------------------------------------------------
add_executable(nereus-audio-capture src/capture_main.cpp)
target_link_libraries(nereus-audio-capture PRIVATE NereusCore)
# The desktop app always carries the helper built from the same revision.
add_dependencies(NereusSDR nereus-audio-capture)
if(APPLE)
    # The macOS POST_BUILD below copies this build-tree binary into
    # NereusSDR.app/Contents/Helpers, so its rpath must find the bundle's
    # private libraries at @executable_path/../Frameworks first, exactly
    # like the NereusSDR executable. The build-tree entries CMake appends
    # after it keep the unbundled binary runnable for tests. The install
    # rpath is set with nereusd's in the Install section below.
    set_target_properties(nereus-audio-capture PROPERTIES
        BUILD_RPATH "@executable_path/../Frameworks")
endif()
# nereus_apply_warnings(nereus-audio-capture) is applied below, after the
# function is defined (next to nereus-media-probe).

# --------------------------------------------------------------------------
# NereusCore / NereusGui dependency application (R1 Task 1; see
# docs/architecture/2026-07-28-remote-daemon-architecture-design.md section 4.1)
#
# NereusCore, NereusSDRLib, and their LTO twins are four distinct targets.
# Rather than hand-duplicating each target_link_libraries /
# target_include_directories / target_compile_definitions call for all four,
# the dependency set each half needs is hoisted into the two functions below
# and applied by calling them once per target. NereusSDRLib/NereusGui_LTO see
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

    # Qt6::Widgets is linked onto NereusCore deliberately, and it is NOT a
    # leftover waiting to be removed.
    #
    # R1 Task 4 ("Extract ISpectrumSink from RadioModel") removed the
    # reason this link was originally load-bearing: RadioModel.cpp no
    # longer includes gui/SpectrumWidget.h, and its former direct
    # m_spectrumWidget->setHighSwrOverlay(...) calls now go through the
    # abstract NereusSDR::ISpectrumSink interface. An earlier version of
    # this comment therefore said the removal was pending "an actual
    # build" verifying nothing else in NereusCore needs Widgets
    # transitively. That audit has now been done (R1 merge-blocker fix
    # round): the only widget-family include anywhere in src/core or
    # src/models is <QDesktopServices> in core/SupportBundle.cpp, and
    # that header is QtGui, not QtWidgets. No source-level need remains.
    #
    # Keep this existing PUBLIC link interface while changing PCH ownership
    # below. The new build-only donor links Widgets independently, so that
    # PCH no longer requires NereusCore itself to link Widgets. Narrowing
    # the Core link interface is a separate dependency-compatibility change.
    #
    # What keeps the invariant honest in the meantime is
    # tests/tst_core_has_no_gui_includes.cpp, which fails on any include
    # from src/core or src/models that reaches into src/gui, into
    # QtWidgets / QtQuick and friends, into the QtGui rendering stack, or
    # into QRhi. That guard, not this link, is the enforcement point.
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

    # R2 Task 17: OpenSSL libcrypto for CertificateStore. PUBLIC so it
    # reaches nereusd (links NereusCore PRIVATE, but PRIVATE only controls
    # whether nereusd's own PUBLIC deps propagate further downstream -- the
    # link itself still lands in the nereusd binary) and every test target
    # via NereusSDRObjs. Unconditional (fix round 1 review, Important 1):
    # find_package(OpenSSL) near the top of this file is REQUIRED, so
    # OpenSSL::Crypto always exists by the time this runs. No
    # HAVE_OPENSSL_CRYPTO compile definition (fix round 3 minor): a prior
    # revision defined one, unconditionally, with nothing anywhere
    # consuming it -- exactly the #ifdef-guarded-call affordance Important
    # 1 warned Task 18 away from reaching for. If a real conditional-
    # compilation need appears later, add the define back at that point,
    # next to its first consumer.
    target_link_libraries(${target} PUBLIC OpenSSL::Crypto)

    # libdatachannel supplies DTLS/SCTP and SRTP transport only. It does not
    # build Opus, so RADE's existing Opus remains the sole codec copy.
    target_link_libraries(${target} PRIVATE
        LibDataChannel::LibDataChannelStatic)

    # iPhone app Task 14: SPAKE2+EE on libsodium, reached only through
    # src/core/security/SpakeExchange.cpp, so no consumer needs its headers.
    target_link_libraries(${target} PRIVATE nereus_spake2ee)

    # iPhone app Task 16 (D36): one Bonjour backend per platform. Each defines
    # createPlatformDnsSdBackend(); without one, DnsSdAdvertiser.cpp supplies
    # a backend that is never available.
    if(APPLE)
        # dns_sd.h and its functions are part of libSystem.
        target_sources(${target} PRIVATE src/core/session/DnsSdAdvertiserApple.cpp)
        target_compile_definitions(${target} PRIVATE NEREUS_HAVE_DNSSD_BACKEND)
    elseif(WIN32)
        # dnsapi.dll is loaded at run time; nothing to link.
        target_sources(${target} PRIVATE src/core/session/DnsSdAdvertiserWindows.cpp)
        target_compile_definitions(${target} PRIVATE NEREUS_HAVE_DNSSD_BACKEND)
        set_source_files_properties(src/core/session/DnsSdAdvertiserWindows.cpp
            PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND Qt6DBus_FOUND)
        target_sources(${target} PRIVATE src/core/session/DnsSdAdvertiserAvahi.cpp)
        target_link_libraries(${target} PRIVATE Qt6::DBus)
        target_compile_definitions(${target} PRIVATE NEREUS_HAVE_DNSSD_BACKEND)
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
#
# The Opus archive and its DRED/OSCE model come from the dependency
# manifest: OPUS_URL is the pre-fetched copy's path when there is one,
# OPUS_URL_HASH pins the archive, and NEREUS_OPUS_MODEL_ARCHIVE hands the
# model to BuildOpus.cmake's vendored patch (5). A -DOPUS_URL from the
# command line (the offline kits' Opus zip with the model) is left alone;
# BuildOpus.cmake then requires -DOPUS_URL_HASH with it.
# --------------------------------------------------------------------------
if(NOT DEFINED OPUS_URL)
    nereus_dependency_archive(opus OPUS_URL OPUS_URL_HASH)
endif()
nereus_dependency_archive_local(opus_model NEREUS_OPUS_MODEL_ARCHIVE)
if(NEREUS_OPUS_MODEL_ARCHIVE)
    # Checked here as well as by the fetch script: a hand-placed file in the
    # directory must not reach a platform whose model script checks nothing.
    file(SHA256 "${NEREUS_OPUS_MODEL_ARCHIVE}" _opus_model_have)
    if(NOT _opus_model_have STREQUAL NEREUS_DEPENDENCY_ARCHIVE_opus_model_SHA256)
        message(FATAL_ERROR
            "${NEREUS_OPUS_MODEL_ARCHIVE} has SHA-256 ${_opus_model_have}, not the pinned "
            "${NEREUS_DEPENDENCY_ARCHIVE_opus_model_SHA256}")
    endif()
endif()
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
    # R-R3-36: the microphone capture helper is found beside NereusSDR.exe
    # (locateCaptureHelper). Both normally build into the same directory,
    # in which case this copy is a no-op; it keeps the layout true for any
    # generator that separates them.
    add_custom_command(TARGET NereusSDR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:nereus-audio-capture>" "$<TARGET_FILE_DIR:NereusSDR>"
        COMMENT "Copying nereus-audio-capture.exe next to NereusSDR.exe")
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
            # The headless package installs only the nereusd component. Keep
            # the model at ModelPaths probe #3 without changing desktop installs.
            install(FILES "${DFNR_MODEL}"
                DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/NereusSDR/models/dfnet3"
                COMPONENT nereusd
                EXCLUDE_FROM_ALL)
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
nereus_apply_gui_deps(NereusSDRLib)
target_link_libraries(NereusSDRLib PUBLIC NereusCore)

# Remote Daemon R3 developer probe. Kept out of routine builds and installs;
# it reads credentials from a private file and links the headless core only.
add_executable(nereus-media-probe EXCLUDE_FROM_ALL
    tools/nereus-media-probe.cpp)
target_link_libraries(nereus-media-probe PRIVATE NereusCore)
nereus_apply_warnings(nereus-media-probe)
nereus_apply_warnings(nereus-audio-capture)

# Digital-mode decode bench codec stage (R-R3-43, R-R3-23). Developer
# measurement tool, kept out of routine builds and installs: runs audio files
# through the Core's own Opus and L16 codec objects for
# tools/digital-mode-opus-bench/run-bench.py.
add_executable(nereus-opus-bench-codec EXCLUDE_FROM_ALL
    tools/digital-mode-opus-bench/opus-bench-codec.cpp)
target_link_libraries(nereus-opus-bench-codec PRIVATE NereusCore)
nereus_apply_warnings(nereus-opus-bench-codec)

# NNR model cost benchmark (R-R3-40). Developer measurement tools, kept out
# of routine builds and installs. nereus-nnr-bench links wdsp_static as the
# application does. nereus-nnr-bench-relaxed links its own copy of nnet.c
# compiled with relaxed floating-point options ahead of wdsp_static, so the
# archive's nnet.c object is never pulled in. The relaxed options apply to
# that OBJECT library only; wdsp_static and the application are unchanged.
if(TARGET wdsp_static AND NOT WIN32)
    add_library(wdsp_nnet_relaxed OBJECT EXCLUDE_FROM_ALL
        third_party/wdsp/src/nnet.c)
    target_include_directories(wdsp_nnet_relaxed PRIVATE
        $<TARGET_PROPERTY:wdsp_static,INCLUDE_DIRECTORIES>)
    target_compile_definitions(wdsp_nnet_relaxed PRIVATE
        $<TARGET_PROPERTY:wdsp_static,COMPILE_DEFINITIONS>)
    target_compile_options(wdsp_nnet_relaxed PRIVATE
        $<TARGET_PROPERTY:wdsp_static,COMPILE_OPTIONS>
        -fno-math-errno -fassociative-math -fno-signed-zeros -fno-trapping-math)

    add_executable(nereus-nnr-bench EXCLUDE_FROM_ALL tools/nereus-nnr-bench.c)
    add_executable(nereus-nnr-bench-relaxed EXCLUDE_FROM_ALL
        tools/nereus-nnr-bench.c $<TARGET_OBJECTS:wdsp_nnet_relaxed>)
    target_compile_definitions(nereus-nnr-bench-relaxed PRIVATE
        NEREUS_NNR_BENCH_RELAXED=1)
    # Plain C: keep the project-wide Qt code generators off these targets.
    set_target_properties(wdsp_nnet_relaxed nereus-nnr-bench nereus-nnr-bench-relaxed
        PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    foreach(_nnr_bench nereus-nnr-bench nereus-nnr-bench-relaxed)
        target_compile_definitions(${_nnr_bench} PRIVATE WDSP_STATIC_BUILD)
        target_include_directories(${_nnr_bench} PRIVATE ${FFTW3_INCLUDE_DIRS})
        target_link_libraries(${_nnr_bench} PRIVATE wdsp_static)
        nereus_apply_warnings(${_nnr_bench})
    endforeach()
endif()

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
            resources/shaders/dss_mesh.vert
            resources/shaders/dss_mesh.frag
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
add_dependencies(nereusd nereus_build_tag)

# Deliberately PRIVATE to the two executable targets, consumed only by
# src/main.cpp and src/server_main.cpp. Every test executable links NereusCore,
# so a generated header reaching that object library would relink all of them
# each time HEAD moved. Confined here, a new commit recompiles the two small
# entry-point translation units and relinks those binaries. Each entry point
# hands its value to BuildIdentity; MainWindow reads it for the GUI title.
target_include_directories(NereusSDR PRIVATE "${CMAKE_BINARY_DIR}/generated")
target_include_directories(nereusd PRIVATE "${CMAKE_BINARY_DIR}/generated")

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
# NereusCore translation units and test targets. The donors use only shared
# Qt/STL headers, without Core-private transport or export definitions. GCC
# rejects a Core-built PCH in GUI because Core's RTC_STATIC is absent there.
# Libraries and test executables also need separate donors: their -fPIC vs
# -fPIE modes and pedantic warning settings differ on Linux.
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
    # These build-only objects are never linked. A library donor preserves
    # PIC/pedantic compilation, and one test donor preserves PIE/nonpedantic
    # compilation. There is no PCH per test and no propagated private or
    # export definitions. CMake's executable POSITION_INDEPENDENT_CODE=ON
    # chooses -fPIE, so an OBJECT donor gets that mode explicitly on Unix.
    set(_nereus_pch_source "${CMAKE_CURRENT_BINARY_DIR}/nereus_shared_pch.cpp")
    file(GENERATE OUTPUT "${_nereus_pch_source}"
        CONTENT "// Build-only translation unit for the shared Qt/STL PCH.\n")
    add_library(nereus_library_pch OBJECT EXCLUDE_FROM_ALL "${_nereus_pch_source}")
    add_library(nereus_test_pch OBJECT EXCLUDE_FROM_ALL "${_nereus_pch_source}")
    foreach(_pch_target IN ITEMS nereus_library_pch nereus_test_pch)
        target_link_libraries(${_pch_target} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
        # PipeWire publishes thread-related compile flags to every consumer.
        # Match those flags without importing Core-private definitions.
        if(PIPEWIRE_FOUND)
            target_compile_options(${_pch_target} PRIVATE ${PIPEWIRE_CFLAGS_OTHER})
        endif()
        set_target_properties(${_pch_target} PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    endforeach()
    if(MSVC)
        target_compile_options(nereus_library_pch PRIVATE /W3 /Zc:__cplusplus)
    else()
        target_compile_options(nereus_library_pch PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(UNIX)
        set_target_properties(nereus_test_pch PROPERTIES POSITION_INDEPENDENT_CODE OFF)
        target_compile_options(nereus_test_pch PRIVATE -fPIE)
    endif()
    set(_nereus_shared_pch_headers
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
    target_precompile_headers(nereus_library_pch PRIVATE ${_nereus_shared_pch_headers})
    target_precompile_headers(nereus_test_pch PRIVATE ${_nereus_shared_pch_headers})
    target_precompile_headers(NereusCore REUSE_FROM nereus_library_pch)
    target_precompile_headers(NereusSDRLib REUSE_FROM nereus_library_pch)
endif()

# --------------------------------------------------------------------------
# Install
# --------------------------------------------------------------------------
include(GNUInstallDirs)

# The vendored RADE codec (third_party/rade/src/CMakeLists.txt) builds
# `rade` as a SHARED library, and both NereusSDR and nereusd link it. It
# had no install() rule of its own, so nothing that `cmake --install`
# produced could actually run: ld.so cannot find librade.so.0.1 outside
# the build tree. The release artifacts hid this because linuxdeploy
# (AppImage) and macdeployqt (DMG) each bundle shared deps by tracing the
# binary, neither of which is involved in a plain install.
#
# Installing it turns install-tree binaries into runnable binaries, and
# is what makes the INSTALL_RPATH below resolvable. It is a no-op for the
# packaged artifacts, which continue to get their copy from the deploy
# tool.
#
# Scoped to the `nereusd` component only, deliberately. The GUI's packaged
# artifacts already get librade from their deploy tool, so a default-component
# copy would buy nothing while perturbing the one path that actually ships:
# at prefix /usr the AppImage job would land a second copy under a multiarch
# libdir alongside linuxdeploy's, with an untested interaction against
# linuxdeploy's rpath rewriting. release.yml is tag-triggered and is the only
# workflow that runs `cmake --install`, so PR CI would never catch a mistake
# there. Keeping the daemon's rules out of the default component leaves this
# branch's effect on the default install exactly nil.
#
# NereusSDR ships an application, not a RADE SDK, so the runtime library
# is installed and the development header is not. Upstream sets
# PUBLIC_HEADER "rade_api.h" on the target but declares no install rule of
# its own, so the property is inert in the vendored build; clearing it here
# keeps install() from warning about a PUBLIC_HEADER with nowhere to go.
# It cannot simply be given a DESTINATION either: the property holds a
# relative path, which install() resolves against the directory of the
# install() call (this file) rather than the target's own directory, so it
# fails to find third_party/rade/src/rade_api.h and aborts the install.
set_target_properties(rade PROPERTIES PUBLIC_HEADER "")

# COMPONENT and EXCLUDE_FROM_ALL are repeated inside each artifact group on
# purpose: install(TARGETS) binds them to the group they follow, so a single
# trailing "COMPONENT nereusd EXCLUDE_FROM_ALL" would apply to RUNTIME only
# and silently leave the ELF/Mach-O LIBRARY artifact in the default
# component, which is exactly the failure this rule exists to prevent.
install(TARGETS rade
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT nereusd EXCLUDE_FROM_ALL
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT nereusd EXCLUDE_FROM_ALL
)

# Point the install-tree daemon at the install-tree librade.
#
# Computed rather than hardcoded to "$ORIGIN/../lib": GNUInstallDirs
# resolves CMAKE_INSTALL_LIBDIR to a multiarch path (lib/x86_64-linux-gnu)
# on Debian-family distros when the prefix is /usr, which the release
# AppImage job uses, so a literal ../lib would miss the library it is
# meant to find.
#
# nereusd only, not NereusSDR. The GUI binary had no INSTALL_RPATH before
# this branch and gets none now, so linuxdeploy's rpath rewriting in the
# AppImage job sees exactly what it saw before. See the rade install rule
# above for why leaving the default install path untouched matters here.
#
# Apple gets the equivalent executable-relative path so a staged component
# can be inspected and run locally even though nereusd is not shipped there.
if(APPLE)
    set_target_properties(nereusd PROPERTIES
        INSTALL_RPATH "@executable_path/../${CMAKE_INSTALL_LIBDIR}"
    )
    # The capture helper: bundle Frameworks first (the Helpers copy), then
    # the nereusd component's bin + lib layout.
    set_target_properties(nereus-audio-capture PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks;@executable_path/../${CMAKE_INSTALL_LIBDIR}"
    )
elseif(UNIX)
    file(RELATIVE_PATH NEREUS_BIN_TO_LIB
         "${CMAKE_INSTALL_FULL_BINDIR}"
         "${CMAKE_INSTALL_FULL_LIBDIR}")
    set_target_properties(nereusd PROPERTIES
        INSTALL_RPATH "$ORIGIN/${NEREUS_BIN_TO_LIB}"
    )
    # The capture helper installs beside nereusd (and beside NereusSDR in
    # the default component) and links NereusCore from the same libdir.
    set_target_properties(nereus-audio-capture PROPERTIES
        INSTALL_RPATH "$ORIGIN/${NEREUS_BIN_TO_LIB}"
    )
endif()

install(TARGETS NereusSDR
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION .
)

# Both application-private libraries travel with the desktop executable.
# The daemon component also installs NereusCore below through its own target
# dependency packaging path.
if(NOT APPLE)
    install(TARGETS NereusCore NereusSDRLib
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# The component install is a standalone daemon runtime. NereusCore is a
# shared library, so it must travel with nereusd just like RADE does above.
# This rule is intentionally separate from the default desktop install.
install(TARGETS NereusCore
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT nereusd EXCLUDE_FROM_ALL
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT nereusd EXCLUDE_FROM_ALL
)

# nereusd -- headless daemon (R1 Task 12; see
# docs/architecture/2026-08-02-remote-daemon-r1-plan.md Task 12).
#
# EXCLUDE_FROM_ALL, so a bare `cmake --install` does NOT install the
# daemon; it takes an explicit `--component nereusd`. This is not a
# preference, it is a release-artifact fix: .github/workflows/release.yml
# runs `DESTDIR=.../AppDir cmake --install build` for the Linux AppImage,
# so without the exclusion this rule and the two below silently push the
# daemon binary plus a systemd unit into the AppDir of a desktop GUI
# artifact, where a systemd unit can never be enabled and the binary is
# unreachable behind AppRun. Excluding at the CMake side fixes it without
# the workflow needing to know the daemon exists.
#
# Install the daemon with:
#   cmake --install build --component nereusd
install(TARGETS nereusd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT nereusd
    EXCLUDE_FROM_ALL
)

# R-R3-21: the Core owns the NR3 model, and "Default" is the bundled
# Default_large.bin file, never an empty path. The desktop packages carry
# both bundled models through release.yml (AppDir share/, .app Resources/,
# Windows models/); nereusd's component had none, so an installed daemon
# could not load any NR3 model. ModelPaths probe #3 finds these at
# <bindir>/../share/NereusSDR/models/rnnoise/.
install(FILES
        "${CMAKE_SOURCE_DIR}/third_party/rnnoise/models/Default_large.bin"
        "${CMAKE_SOURCE_DIR}/third_party/rnnoise/models/Default_small.bin"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/NereusSDR/models/rnnoise"
    COMPONENT nereusd
    EXCLUDE_FROM_ALL
)

# nereus-audio-capture -- the microphone helper (R-R3-36). locateCaptureHelper()
# looks for it beside its parent executable, so the default component
# (not macOS, where POST_BUILD places it inside NereusSDR.app/Contents/Helpers)
# installs it beside NereusSDR, and the Linux AppImage's "cmake --install"
# into AppDir picks it up.
#
# The nereusd component deliberately carries no helper. The daemon never
# opens a microphone (DaemonApp calls setPcCaptureAllowed(false)), nereusd
# does not depend on the helper target, and a daemon-only build
# ("cmake --build --target nereusd") never produces the binary, so an
# install rule here would make "cmake --install --component nereusd" fail
# and would ship a microphone helper the daemon cannot use.
if(NOT APPLE)
    install(TARGETS nereus-audio-capture
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

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

    # ExecStart= and the bring-up instructions in the unit's own header
    # used to be hardcoded to this project's default /usr/local prefix,
    # with a comment saying a differently-prefixed package "will need this
    # path generated instead". Generate it now: the release AppImage job
    # already configures with -DCMAKE_INSTALL_PREFIX=/usr, so the
    # hardcoded /usr/local was already wrong for the one non-default
    # prefix this repository builds with.
    configure_file(packaging/nereusd.service.in
                   "${CMAKE_CURRENT_BINARY_DIR}/generated/nereusd.service"
                   @ONLY)

    # COMPONENT nereusd + EXCLUDE_FROM_ALL for the same reason as the
    # nereusd target above: keep the daemon out of a plain
    # `cmake --install`, which the AppImage job runs.
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/nereusd.service"
            DESTINATION "${NEREUSD_SYSTEMD_UNIT_DIR}"
            COMPONENT nereusd
            EXCLUDE_FROM_ALL)
    install(FILES packaging/nereusd.conf.sample
            DESTINATION "${CMAKE_INSTALL_DATADIR}/nereusd"
            COMPONENT nereusd
            EXCLUDE_FROM_ALL)
    # Source-stage installers for both ARM stations use the same bounded
    # artifact cleanup. Ship the helper with every daemon stage so a fresh
    # install does not depend on a separately provisioned maintenance file.
    install(PROGRAMS scripts/radxa/prune-core-artifacts.py
            DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/nereusd"
            COMPONENT nereusd
            EXCLUDE_FROM_ALL)
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
# R-R3-50: the Core's install (--component nereusd) carries the same texts,
# since nereusd ships most of the same libraries. EXCLUDE_FROM_ALL keeps a
# plain `cmake --install` exactly as it was; the licenses component above
# already installs these files to the same place.
install(DIRECTORY packaging/third-party-licenses/
        DESTINATION share/doc/nereussdr/licenses
        COMPONENT nereusd
        EXCLUDE_FROM_ALL)
install(FILES LICENSE LICENSE-DUAL-LICENSING
        DESTINATION share/doc/nereussdr
        COMPONENT nereusd
        EXCLUDE_FROM_ALL)

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

# --------------------------------------------------------------------------
# macOS: sign the app bundle once, after everything is in it (R-R3-50).
#
# POST_BUILD commands on a target run in the order they are added, so this
# block stays the LAST add_custom_command(TARGET NereusSDR POST_BUILD) in
# the configure: the private libraries, the capture helper and the
# Info.plist strings (staged above, after set_target_properties) and the
# DeepFilterNet3 model (the ENABLE_DFNR block) are all in the bundle by
# the time it runs. Signing earlier left the model outside the seal, and
# `codesign --verify --deep --strict` failed with "a sealed resource is
# missing or invalid".
#
# Nested code is signed inside out and keeps its own signature: the two
# private libraries, then the capture helper with its entitlements
# (disable-library-validation, audio-input), then the outer bundle, which
# seals the nested signatures without re-signing them. No --deep: a --deep
# re-sign of the bundle replaces every nested signature with one that
# carries no entitlements, which stripped the helper's microphone
# entitlement. Ad hoc identity ("-"), as before; the release workflow
# signs again with its own identity after macdeployqt.
# --------------------------------------------------------------------------
if(APPLE)
    add_custom_command(TARGET NereusSDR POST_BUILD
        COMMAND codesign --force --sign - "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Frameworks/$<TARGET_FILE_NAME:NereusCore>"
        COMMAND codesign --force --sign - "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Frameworks/$<TARGET_FILE_NAME:NereusSDRLib>"
        COMMAND codesign --force --sign - --entitlements "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/app-entitlements.plist" "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Helpers/nereus-audio-capture"
        COMMAND codesign --force --sign - "$<TARGET_BUNDLE_CONTENT_DIR:NereusSDR>/Helpers/NereusStation.app"
        COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:NereusSDR>"
        COMMENT "Sign the private libraries, both helpers, and then the app bundle"
        VERBATIM
    )
endif()
