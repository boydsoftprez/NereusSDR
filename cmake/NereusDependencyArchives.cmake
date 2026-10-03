# =================================================================
# cmake/NereusDependencyArchives.cmake  (NereusSDR)
# =================================================================
#
# no-port-check: NereusSDR-original.
#
# Every source archive and git repository a NereusSDR configure or build
# downloads, with its pin. One list serves two readers:
#
#   * the CMake declarations (cmake/NereusPairing.cmake,
#     cmake/NereusRemoteMedia.cmake, the Opus build in third_party/rade and
#     the Windows FFTW fallback in CMakeLists.txt) take each archive's URL
#     and SHA-256 from here through nereus_dependency_archive();
#   * cmake/FetchDependencyArchives.cmake (run with cmake -P) downloads the
#     same archives and mirrors the same git repositories into one
#     directory, with retries, verifying every archive's SHA-256 and every
#     repository's pinned commit. CI caches that directory across runs.
#
# When NEREUS_DEPENDENCY_ARCHIVE_DIR names that directory and an archive is
# in it, the declarations use the local copy in place of the upstream URL
# (ExternalProject and FetchContent take one local path, never a path in a
# URL list). CMake still checks URL_HASH on it, so a local copy that does
# not match the pin fails the build exactly as a bad download would;
# nothing is ever used unverified. An archive missing from the directory,
# or no directory (any ordinary local build), leaves the declaration as it
# was: the upstream URL and its hash.
#
# The git repositories are declared with GIT_REPOSITORY/GIT_TAG where they
# are used (CMakeLists.txt, third_party/rnnoise, third_party/libspecbleach).
# They are listed here only for the fetch script, which checks that each
# declaring file still names the URL and ref recorded here, so the two
# cannot drift apart silently.
#
# Modification history (NereusSDR):
#   2026-10-01: Created. A GitHub 504 on the libsodium and Opus archive
#               downloads failed two Linux CI jobs (run 36866003880).
#               J.J. Boyd (KG4VCF), with AI-assisted implementation via
#               Anthropic Claude Code.
# =================================================================

include_guard(GLOBAL)

# ── Archives ─────────────────────────────────────────────────────────────
# <name>_FILE      the file name in NEREUS_DEPENDENCY_ARCHIVE_DIR
# <name>_URL       the upstream URL, unchanged from the original declaration
# <name>_SHA256    the archive's SHA-256, unchanged from the original
#                  declaration (Opus and the FFTW zip had none; see below)
# <name>_PLATFORMS which hosts' CI jobs fetch it (Linux, macOS, Windows)
set(NEREUS_DEPENDENCY_ARCHIVES
    libsodium spake2ee
    libdatachannel plog usrsctp libjuice json libsrtp
    opus opus_model
    fftw_win64)

# cmake/NereusPairing.cmake
set(NEREUS_DEPENDENCY_ARCHIVE_libsodium_FILE      libsodium-1.0.22.tar.gz)
set(NEREUS_DEPENDENCY_ARCHIVE_libsodium_URL       https://github.com/jedisct1/libsodium/releases/download/1.0.22-RELEASE/libsodium-1.0.22.tar.gz)
set(NEREUS_DEPENDENCY_ARCHIVE_libsodium_SHA256    adbdd8f16149e81ac6078a03aca6fc03b592b89ef7b5ed83841c086191be3349)
set(NEREUS_DEPENDENCY_ARCHIVE_libsodium_PLATFORMS Linux macOS Windows)

set(NEREUS_DEPENDENCY_ARCHIVE_spake2ee_FILE      spake2-ee-fd3ea61f27a75ff63b0f192c9e619b5a494d048e.tar.gz)
set(NEREUS_DEPENDENCY_ARCHIVE_spake2ee_URL       https://codeload.github.com/jedisct1/spake2-ee/tar.gz/fd3ea61f27a75ff63b0f192c9e619b5a494d048e)
set(NEREUS_DEPENDENCY_ARCHIVE_spake2ee_SHA256    20d63587c1191b952e98b9a4d8bd557c8a6c4f6bfbac0b9fb77b28854c244bb6)
set(NEREUS_DEPENDENCY_ARCHIVE_spake2ee_PLATFORMS Linux macOS Windows)

# cmake/NereusRemoteMedia.cmake
set(NEREUS_DEPENDENCY_ARCHIVE_libdatachannel_FILE      libdatachannel-v0.24.5.tar.gz)
set(NEREUS_DEPENDENCY_ARCHIVE_libdatachannel_URL       https://codeload.github.com/paullouisageneau/libdatachannel/tar.gz/refs/tags/v0.24.5)
set(NEREUS_DEPENDENCY_ARCHIVE_libdatachannel_SHA256    454537c3cd526bed935d847bb2dff4046f266eef84d43b2a5f2f2f293c0026f4)
set(NEREUS_DEPENDENCY_ARCHIVE_libdatachannel_PLATFORMS Linux macOS Windows)

set(NEREUS_DEPENDENCY_ARCHIVE_plog_FILE      plog-94899e0b926ac1b0f4750bfbd495167b4a6ae9ef.tar.gz)
set(NEREUS_DEPENDENCY_ARCHIVE_plog_URL       https://codeload.github.com/SergiusTheBest/plog/tar.gz/94899e0b926ac1b0f4750bfbd495167b4a6ae9ef)
set(NEREUS_DEPENDENCY_ARCHIVE_plog_SHA256    92a08bce559b5f28aa88d3fd9071567414b9f43a83fe2a05a6dd14f1da536072)
set(NEREUS_DEPENDENCY_ARCHIVE_plog_PLATFORMS Linux macOS Windows)

set(NEREUS_DEPENDENCY_ARCHIVE_usrsctp_FILE      usrsctp-fec583d54493f879d2ae44a743423bf8a04371ab.tar.gz)
set(NEREUS_DEPENDENCY_ARCHIVE_usrsctp_URL       https://codeload.github.com/paullouisageneau/usrsctp/tar.gz/fec583d54493f879d2ae44a743423bf8a04371ab)
set(NEREUS_DEPENDENCY_ARCHIVE_usrsctp_SHA256    e5c114afe73c9a0ec419fab5f5b3f63f3ce57b09b90d06ea85e63dca8aed3e7c)
set(NEREUS_DEPENDENCY_ARCHIVE_usrsctp_PLATFORMS Linux macOS Windows)

set(NEREUS_DEPENDENCY_ARCHIVE_libjuice_FILE      libjuice-3c40a3545b6b1b62c7adee7f8f2bd58aa290afd6.tar.gz)
set(NEREUS_DEPENDENCY_ARCHIVE_libjuice_URL       https://codeload.github.com/paullouisageneau/libjuice/tar.gz/3c40a3545b6b1b62c7adee7f8f2bd58aa290afd6)
set(NEREUS_DEPENDENCY_ARCHIVE_libjuice_SHA256    a6b1d55338ea12adc0177eaafd9521ac0101b6a8716c71024a668f4896fd6b7c)
set(NEREUS_DEPENDENCY_ARCHIVE_libjuice_PLATFORMS Linux macOS Windows)

set(NEREUS_DEPENDENCY_ARCHIVE_json_FILE      json-55f93686c01528224f448c19128836e7df245f72.tar.gz)
set(NEREUS_DEPENDENCY_ARCHIVE_json_URL       https://codeload.github.com/nlohmann/json/tar.gz/55f93686c01528224f448c19128836e7df245f72)
set(NEREUS_DEPENDENCY_ARCHIVE_json_SHA256    67f4cdd9ca930c9c1e130af4a437c7fc98fab77a2846fc2d2a14b4943831f8ef)
set(NEREUS_DEPENDENCY_ARCHIVE_json_PLATFORMS Linux macOS Windows)

set(NEREUS_DEPENDENCY_ARCHIVE_libsrtp_FILE      libsrtp-24b3bf8f19b6f5ab4cd2bcceb4f4064efca86fd5.tar.gz)
set(NEREUS_DEPENDENCY_ARCHIVE_libsrtp_URL       https://codeload.github.com/cisco/libsrtp/tar.gz/24b3bf8f19b6f5ab4cd2bcceb4f4064efca86fd5)
set(NEREUS_DEPENDENCY_ARCHIVE_libsrtp_SHA256    063478e368d7cd13d04a908d152a46f90bfa728c4b324be6d413b0b92728207c)
set(NEREUS_DEPENDENCY_ARCHIVE_libsrtp_PLATFORMS Linux macOS Windows)

# third_party/rade/cmake/BuildOpus.cmake. The URL is radae_nopy's own
# OPUS_URL. Upstream declares no hash; the SHA-256 here is the archive
# GitHub served for that commit on 2025-09-28 (the copy in a local build
# tree from then) and again on 2026-10-01, identical.
set(NEREUS_DEPENDENCY_ARCHIVE_opus_FILE      opus-940d4e5af64351ca8ba8390df3f555484c567fbb.zip)
set(NEREUS_DEPENDENCY_ARCHIVE_opus_URL       https://github.com/xiph/opus/archive/940d4e5af64351ca8ba8390df3f555484c567fbb.zip)
set(NEREUS_DEPENDENCY_ARCHIVE_opus_SHA256    20e37f9079ac2b80e3235cd8ce2547829e147bf44a2f5dd28a888fa3e9c24341)
set(NEREUS_DEPENDENCY_ARCHIVE_opus_PLATFORMS Linux macOS Windows)

# The DRED/OSCE model the pinned Opus downloads from its autogen.sh (via
# dnn/download_model.sh) or, on Windows, dnn/download_model.bat. Its name
# is its SHA-256, the hash autogen.sh passes and BuildOpus.cmake's
# _opus_model_sha repeats. The file name must stay exactly the one those
# scripts look for: a copy already in the Opus source tree is used instead
# of downloading.
set(NEREUS_DEPENDENCY_ARCHIVE_opus_model_FILE      opus_data-4ed9445b96698bad25d852e912b41495ddfa30c8dbc8a55f9cde5826ed793453.tar.gz)
set(NEREUS_DEPENDENCY_ARCHIVE_opus_model_URL       https://media.xiph.org/opus/models/opus_data-4ed9445b96698bad25d852e912b41495ddfa30c8dbc8a55f9cde5826ed793453.tar.gz)
set(NEREUS_DEPENDENCY_ARCHIVE_opus_model_SHA256    4ed9445b96698bad25d852e912b41495ddfa30c8dbc8a55f9cde5826ed793453)
set(NEREUS_DEPENDENCY_ARCHIVE_opus_model_PLATFORMS Linux macOS Windows)

# The Windows FFTW double-precision DLL (ci.yml "Download FFTW3 (Windows)"
# and the configure-time fallback in CMakeLists.txt). Neither declared a
# hash; this is the SHA-256 of the zip fftw.org served on 2026-10-01.
set(NEREUS_DEPENDENCY_ARCHIVE_fftw_win64_FILE      fftw-3.3.5-dll64.zip)
set(NEREUS_DEPENDENCY_ARCHIVE_fftw_win64_URL       https://fftw.org/pub/fftw/fftw-3.3.5-dll64.zip)
set(NEREUS_DEPENDENCY_ARCHIVE_fftw_win64_SHA256    cfd88dc0e8d7001115ea79e069a2c695d52c8947f5b4f3b7ac54a192756f439f)
set(NEREUS_DEPENDENCY_ARCHIVE_fftw_win64_PLATFORMS Windows)

# ── Git repositories ─────────────────────────────────────────────────────
# <name>_URL         GIT_REPOSITORY as declared
# <name>_REF         GIT_TAG as declared (a tag or a commit)
# <name>_COMMIT      the commit that ref must resolve to
# <name>_DECLARED_IN the file holding the declaration, from the source root
# <name>_PLATFORMS   which hosts' CI jobs fetch it
# libASPL (hal-plugin/CMakeLists.txt) is left out: the HAL plug-in is off
# unless NEREUSSDR_BUILD_HAL_PLUGIN is set, and no CI job sets it.
set(NEREUS_DEPENDENCY_GIT_REPOS portaudio rnnoise libspecbleach zlib)

set(NEREUS_DEPENDENCY_GIT_portaudio_URL         https://github.com/PortAudio/portaudio.git)
set(NEREUS_DEPENDENCY_GIT_portaudio_REF         v19.7.0)
set(NEREUS_DEPENDENCY_GIT_portaudio_COMMIT      147dd722548358763a8b649b3e4b41dfffbcfbb6)
set(NEREUS_DEPENDENCY_GIT_portaudio_DECLARED_IN CMakeLists.txt)
set(NEREUS_DEPENDENCY_GIT_portaudio_PLATFORMS   Linux macOS Windows)

set(NEREUS_DEPENDENCY_GIT_rnnoise_URL         https://gitlab.xiph.org/xiph/rnnoise.git)
set(NEREUS_DEPENDENCY_GIT_rnnoise_REF         70f1d256acd4b34a572f999a05c87bf00b67730d)
set(NEREUS_DEPENDENCY_GIT_rnnoise_COMMIT      70f1d256acd4b34a572f999a05c87bf00b67730d)
set(NEREUS_DEPENDENCY_GIT_rnnoise_DECLARED_IN third_party/rnnoise/CMakeLists.txt)
set(NEREUS_DEPENDENCY_GIT_rnnoise_PLATFORMS   Linux macOS Windows)

set(NEREUS_DEPENDENCY_GIT_libspecbleach_URL         https://github.com/lucianodato/libspecbleach.git)
set(NEREUS_DEPENDENCY_GIT_libspecbleach_REF         41d3f58310391e05ecfb8b7c9efb62ea2ba8ef05)
set(NEREUS_DEPENDENCY_GIT_libspecbleach_COMMIT      41d3f58310391e05ecfb8b7c9efb62ea2ba8ef05)
set(NEREUS_DEPENDENCY_GIT_libspecbleach_DECLARED_IN third_party/libspecbleach/CMakeLists.txt)
set(NEREUS_DEPENDENCY_GIT_libspecbleach_PLATFORMS   Linux macOS Windows)

set(NEREUS_DEPENDENCY_GIT_zlib_URL         https://github.com/madler/zlib.git)
set(NEREUS_DEPENDENCY_GIT_zlib_REF         v1.3.1)
set(NEREUS_DEPENDENCY_GIT_zlib_COMMIT      51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf)
set(NEREUS_DEPENDENCY_GIT_zlib_DECLARED_IN CMakeLists.txt)
set(NEREUS_DEPENDENCY_GIT_zlib_PLATFORMS   Windows)

# ── Where the local copies are ───────────────────────────────────────────
# A configure takes the directory from -DNEREUS_DEPENDENCY_ARCHIVE_DIR or,
# on its first run, from the environment variable of the same name (which
# is how CI hands it to every configure in a job). Empty means none.
if(NOT CMAKE_SCRIPT_MODE_FILE)
    set(NEREUS_DEPENDENCY_ARCHIVE_DIR "$ENV{NEREUS_DEPENDENCY_ARCHIVE_DIR}"
        CACHE PATH
        "Directory of pre-fetched, hash-checked dependency archives (cmake/FetchDependencyArchives.cmake); empty for none")
endif()

# The local path of archive <name> in NEREUS_DEPENDENCY_ARCHIVE_DIR, or ""
# when there is no directory or the archive is not in it.
function(nereus_dependency_archive_local name out_path)
    set(_path "")
    if(NOT DEFINED NEREUS_DEPENDENCY_ARCHIVE_${name}_FILE)
        message(FATAL_ERROR "nereus_dependency_archive: no archive named '${name}' in cmake/NereusDependencyArchives.cmake")
    endif()
    if(NEREUS_DEPENDENCY_ARCHIVE_DIR)
        file(TO_CMAKE_PATH "${NEREUS_DEPENDENCY_ARCHIVE_DIR}" _dir)
        if(EXISTS "${_dir}/${NEREUS_DEPENDENCY_ARCHIVE_${name}_FILE}")
            set(_path "${_dir}/${NEREUS_DEPENDENCY_ARCHIVE_${name}_FILE}")
        endif()
    endif()
    set(${out_path} "${_path}" PARENT_SCOPE)
endfunction()

# The URL and URL_HASH value for archive <name>: the local copy's path when
# there is one, else the upstream URL. ExternalProject and FetchContent
# check the hash on a local path as on a download.
function(nereus_dependency_archive name out_url out_hash)
    nereus_dependency_archive_local(${name} _local)
    if(_local)
        set(_url "${_local}")
    else()
        set(_url "${NEREUS_DEPENDENCY_ARCHIVE_${name}_URL}")
    endif()
    set(${out_url} "${_url}" PARENT_SCOPE)
    set(${out_hash} "SHA256=${NEREUS_DEPENDENCY_ARCHIVE_${name}_SHA256}" PARENT_SCOPE)
endfunction()
