# =================================================================
# cmake/NereusRemoteMedia.cmake  (NereusSDR)
# =================================================================
#
# no-port-check: NereusSDR-original. Remote-daemon R3 Task 1.
#
# Fetch the audited libdatachannel v0.24.5 source and the exact transitive
# revisions recorded by that tree. Codeload archives are used because they
# are content-addressed here and do not require github.com git transport.
#
# Scope boundary: this R3 build selects libjuice, which is appropriate for
# direct host ICE and TURN/UDP. Pinned libdatachannel v0.24.5 explicitly
# rejects TURN TCP and TURN TLS in src/impl/icetransport.cpp when USE_NICE is
# false. This is therefore not R5 relay acceptance and does not prove the
# required TLS/TCP 443 fallback. IMediaTransport keeps signalling and media
# backend details outside its API so a later libnice or relay adapter can be
# selected without changing session callers. DTLS-SRTP over TURN/UDP must not
# be described as client-to-relay TLS transport.
#
# Modification history (NereusSDR):
#   2026-09-26: iPhone app plan Task 28 (R-IOS-16): libjuice gives its TURN
#               allocations back when an agent is destroyed
#               (nereus_patch_libjuice_turn_release(), the change in
#               cmake/patches/libjuice-release-turn-allocations.c, since
#               replaced by the .patch files of the 2026-09-27 Task 56 entry
#               below). J.J. Boyd (KG4VCF), with AI-assisted implementation
#               via Anthropic Claude Code.
#   2026-09-26: Task 28 fix wave (review Minor 6): agent.c is found among
#               the targets' sources by the file it names, not its spelling.
#               J.J. Boyd (KG4VCF), with AI-assisted implementation via
#               Anthropic Claude Code.
#   2026-09-27: R-R3-49: libdatachannel keeps a remote description before
#               its ICE agent takes it (nereus_patch_libdatachannel_remote_
#               description_first(), the change in cmake/patches/
#               libdatachannel-keep-remote-description-first.cpp). J.J. Boyd
#               (KG4VCF), with AI-assisted implementation via Anthropic
#               Claude Code.
#   2026-09-27: R-R3-49: usrsctp compiles without NOMINMAX under MinGW, so
#               windows.h gives it the min() and max() it uses (PR #327's
#               Windows link). J.J. Boyd (KG4VCF), with AI-assisted
#               implementation via Anthropic Claude Code.
#   2026-09-27: R-R3-49: libdatachannel sets the DTLS MTU before it takes
#               incoming records (nereus_patch_libdatachannel_dtls_mtu_
#               first(), the change in cmake/patches/libdatachannel-set-
#               dtls-mtu-before-incoming.cpp); both libdatachannel changes
#               share _nereus_patch_libdatachannel_source(). J.J. Boyd
#               (KG4VCF), with AI-assisted implementation via Anthropic
#               Claude Code.
#   2026-09-27: Task 56: replace the one-datagram overlay with the reviewed
#               bounded TURN release and ICE retirement patches on build-owned
#               vendor sources.
#               J.J. Boyd (KG4VCF), with AI-assisted implementation via
#               OpenAI Codex.
#   2026-09-30: fix wave (INFRA minor 7): the 2026-09-26 entry names the
#               file the Task 56 patches replaced. J.J. Boyd (KG4VCF), with
#               AI-assisted implementation via Anthropic Claude Code.
#   2026-10-01: the archive URLs and SHA-256 pins move, unchanged, into
#               cmake/NereusDependencyArchives.cmake, which substitutes a
#               pre-fetched, hash-checked local copy when CI provides
#               one (a GitHub 504 failed CI run 36866003880). J.J. Boyd
#               (KG4VCF), with AI-assisted implementation via Anthropic
#               Claude Code.
#
#   2026-10-03: keep the libjuice poll worker alive while finished agents
#               retain its registry, via libjuice-0003. J.J. Boyd (KG4VCF),
#               with AI-assisted implementation via OpenAI Codex.
#
# =================================================================

include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/NereusDependencyArchives.cmake")
find_package(Git REQUIRED)

set(_NEREUS_REMOTE_MEDIA_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Apply a pinned vendor patch to a build-owned source tree. A failed context
# check is fatal: silently compiling the original code would leak allocations.
function(_nereus_apply_vendor_patch source_dir patch_name)
    set(_patch "${_NEREUS_REMOTE_MEDIA_CMAKE_DIR}/patches/${patch_name}")
    # Git otherwise discovers the enclosing NereusSDR checkout when the
    # build is under it, and can report success while skipping diff --git
    # paths outside its current subdirectory. Stop discovery at the source
    # tree's parent so patch paths are relative to this vendor copy.
    get_filename_component(_ceiling "${source_dir}" DIRECTORY)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
            "GIT_CEILING_DIRECTORIES=${_ceiling}" "LC_ALL=C" "${GIT_EXECUTABLE}"
            apply --check --verbose "${_patch}"
        WORKING_DIRECTORY "${source_dir}" RESULT_VARIABLE _check
        OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    string(FIND "${_out}${_err}" "Checking patch" _checked)
    if(NOT _check EQUAL 0 OR _checked EQUAL -1)
        message(FATAL_ERROR "${patch_name} does not apply to ${source_dir}: ${_out}${_err}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
            "GIT_CEILING_DIRECTORIES=${_ceiling}" "LC_ALL=C" "${GIT_EXECUTABLE}"
            apply --verbose "${_patch}"
        WORKING_DIRECTORY "${source_dir}" RESULT_VARIABLE _result
        OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    string(FIND "${_out}${_err}" "Applied patch" _applied)
    if(NOT _result EQUAL 0 OR _applied EQUAL -1)
        message(FATAL_ERROR "Could not apply ${patch_name}: ${_out}${_err}")
    endif()
endfunction()

# R-R3-49: compiles libdatachannel's targets (datachannel and
# datachannel-static) from a copy of `relative` (a path under `dc_dir`) in
# the build tree, with the one `anchor` in it replaced by the contents of
# cmake/patches/`change`. The fetched source is left as it is; the copy is
# rewritten only when its content changes. A pinned libdatachannel without
# exactly one place for the change stops the configure rather than build
# without it.
function(_nereus_patch_libdatachannel_source dc_dir relative anchor change)
    set(_source "${dc_dir}/${relative}")
    file(READ "${_source}" _text)
    file(READ "${_NEREUS_REMOTE_MEDIA_CMAKE_DIR}/patches/${change}" _replacement)
    string(FIND "${_text}" "${anchor}" _at)
    string(FIND "${_text}" "${anchor}" _last REVERSE)
    if(_at EQUAL -1 OR NOT _at EQUAL _last)
        message(FATAL_ERROR
            "libdatachannel ${_source} does not have exactly one place for the change in "
            "cmake/patches/${change}; update it for this libdatachannel.")
    endif()
    string(REPLACE "${anchor}" "${_replacement}" _text "${_text}")

    set(_patched "${CMAKE_BINARY_DIR}/_deps/nereus-patched/libdatachannel/${relative}")
    set(_old "")
    if(EXISTS "${_patched}")
        file(READ "${_patched}" _old)
    endif()
    if(NOT _old STREQUAL _text)
        file(WRITE "${_patched}" "${_text}")
    endif()

    file(REAL_PATH "${_source}" _source_real)
    get_filename_component(_source_dir "${_source_real}" DIRECTORY)
    set(_swapped 0)
    foreach(_target IN ITEMS datachannel datachannel-static)
        if(TARGET ${_target})
            get_target_property(_sources ${_target} SOURCES)
            get_target_property(_target_dir ${_target} SOURCE_DIR)
            set(_index -1)
            set(_position 0)
            foreach(_entry IN LISTS _sources)
                if(_index EQUAL -1 AND NOT _entry MATCHES "^\\$<")
                    file(REAL_PATH "${_entry}" _entry_real BASE_DIRECTORY "${_target_dir}")
                    if(_entry_real STREQUAL _source_real)
                        set(_index ${_position})
                    endif()
                endif()
                math(EXPR _position "${_position} + 1")
            endforeach()
            if(_index EQUAL -1)
                message(FATAL_ERROR
                    "libdatachannel target ${_target} does not compile ${_source}")
            endif()
            list(REMOVE_AT _sources ${_index})
            list(INSERT _sources ${_index} "${_patched}")
            set_property(TARGET ${_target} PROPERTY SOURCES ${_sources})
            # The copy's own quoted includes ("dtlstransport.hpp") are found
            # beside the original, as they were before it moved.
            set_source_files_properties("${_patched}" TARGET_DIRECTORY ${_target}
                PROPERTIES INCLUDE_DIRECTORIES "${_source_dir}")
            math(EXPR _swapped "${_swapped} + 1")
        endif()
    endforeach()
    if(_swapped EQUAL 0)
        message(FATAL_ERROR "no libdatachannel target to compile ${_patched} into")
    endif()
endfunction()

# R-R3-49: libdatachannel v0.24.5's PeerConnection::setRemoteDescription()
# gives the ICE agent the remote description before it keeps it for the
# DTLS fingerprint check, and a handshake that reaches the check in between
# fails. On a busy computer an offerer taking its answer lost that race
# (the Core's control channel and media alike: "DTLS alert: unknown CA").
# The copy of src/peerconnection.cpp has the two calls in the other order
# (the change and its notice are
# cmake/patches/libdatachannel-keep-remote-description-first.cpp).
function(nereus_patch_libdatachannel_remote_description_first dc_dir)
    _nereus_patch_libdatachannel_source("${dc_dir}" "src/peerconnection.cpp"
        "\ticeTransport->setRemoteDescription(description); // ICE transport might reject the description\n\n\timpl()->processRemoteDescription(std::move(description));\n"
        "libdatachannel-keep-remote-description-first.cpp")
endfunction()

# R-R3-49: libdatachannel v0.24.5's OpenSSL DtlsTransport::start() takes
# incoming records before it sets the DTLS MTU, and a ClientHello handled in
# between runs the handshake with no MTU: "DTLS recv: Handshake failed:
# fatal I/O error", so the control channel or the media peer did not open.
# The copy of src/impl/dtlstransport.cpp sets the MTU first (the change and
# its notice are cmake/patches/libdatachannel-set-dtls-mtu-before-incoming.cpp).
function(nereus_patch_libdatachannel_dtls_mtu_first dc_dir)
    _nereus_patch_libdatachannel_source("${dc_dir}" "src/impl/dtlstransport.cpp"
        "\tPLOG_DEBUG << \"Starting DTLS transport\";\n\tregisterIncoming();\n\tchangeState(State::Connecting);\n\n\tint ret, err;\n\t{\n\t\tstd::lock_guard lock(mSslMutex);\n\n\t\tsize_t mtu = mMtu.value_or(DEFAULT_MTU) - 8 - 40; // UDP/IPv6\n\t\tSSL_set_mtu(mSsl, static_cast<unsigned int>(mtu));\n\t\tPLOG_VERBOSE << \"DTLS MTU set to \" << mtu;\n\n"
        "libdatachannel-set-dtls-mtu-before-incoming.cpp")
endfunction()

function(nereus_add_remote_media_dependency)
    if(TARGET LibDataChannel::LibDataChannelStatic)
        return()
    endif()

    nereus_dependency_archive(libdatachannel _libdatachannel_url _libdatachannel_hash)
    FetchContent_Declare(nereus_libdatachannel
        URL "${_libdatachannel_url}"
        URL_HASH "${_libdatachannel_hash}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR __nereus_no_add_subdirectory)
    nereus_dependency_archive(plog _plog_url _plog_hash)
    FetchContent_Declare(nereus_plog
        URL "${_plog_url}"
        URL_HASH "${_plog_hash}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR __nereus_no_add_subdirectory)
    nereus_dependency_archive(usrsctp _usrsctp_url _usrsctp_hash)
    FetchContent_Declare(nereus_usrsctp
        URL "${_usrsctp_url}"
        URL_HASH "${_usrsctp_hash}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR __nereus_no_add_subdirectory)
    nereus_dependency_archive(libjuice _libjuice_url _libjuice_hash)
    FetchContent_Declare(nereus_libjuice
        URL "${_libjuice_url}"
        URL_HASH "${_libjuice_hash}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR __nereus_no_add_subdirectory)
    nereus_dependency_archive(json _json_url _json_hash)
    FetchContent_Declare(nereus_json
        URL "${_json_url}"
        URL_HASH "${_json_hash}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR __nereus_no_add_subdirectory)
    nereus_dependency_archive(libsrtp _libsrtp_url _libsrtp_hash)
    FetchContent_Declare(nereus_libsrtp
        URL "${_libsrtp_url}"
        URL_HASH "${_libsrtp_hash}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR __nereus_no_add_subdirectory)

    FetchContent_MakeAvailable(
        nereus_libdatachannel nereus_plog nereus_usrsctp
        nereus_libjuice nereus_json nereus_libsrtp)
    set(NEREUS_ORIGINAL_JUICE_SOURCE_DIR "${nereus_libjuice_SOURCE_DIR}" PARENT_SCOPE)
    set(NEREUS_ORIGINAL_DC_SOURCE_DIR "${nereus_libdatachannel_SOURCE_DIR}" PARENT_SCOPE)

    # Work on a build-owned copy. FetchContent source overrides may point at
    # shared offline caches, so never patch or populate their source trees.
    set(_dc_build_source "${CMAKE_BINARY_DIR}/_deps/nereus-built-libdatachannel")
    file(MAKE_DIRECTORY "${_dc_build_source}")
    file(COPY "${nereus_libdatachannel_SOURCE_DIR}/CMakeLists.txt"
              "${nereus_libdatachannel_SOURCE_DIR}/LICENSE"
              "${nereus_libdatachannel_SOURCE_DIR}/cmake"
              "${nereus_libdatachannel_SOURCE_DIR}/include"
              "${nereus_libdatachannel_SOURCE_DIR}/src"
         DESTINATION "${_dc_build_source}")

    foreach(_dep IN ITEMS plog usrsctp libjuice json libsrtp)
        file(REMOVE_RECURSE "${_dc_build_source}/deps/${_dep}")
        file(MAKE_DIRECTORY "${_dc_build_source}/deps/${_dep}")
        file(COPY "${nereus_${_dep}_SOURCE_DIR}/"
             DESTINATION "${_dc_build_source}/deps/${_dep}")
    endforeach()

    # The libjuice copy belongs to this build, so the fetched pin remains
    # pristine. Patch before add_subdirectory so all C sources and consumers
    # see the new internal and public headers together.
    _nereus_apply_vendor_patch("${_dc_build_source}/deps/libjuice"
        "libjuice-0001-give-turn-allocations-back.patch")
    _nereus_apply_vendor_patch("${_dc_build_source}/deps/libjuice"
        "libjuice-0002-bounded-turn-release-lifecycle.patch")
    _nereus_apply_vendor_patch("${_dc_build_source}/deps/libjuice"
        "libjuice-0003-keep-poll-worker-with-retained-agents.patch")
    _nereus_apply_vendor_patch("${_dc_build_source}"
        "libdatachannel-0003-retain-juice-agent-through-turn-release.patch")
    _nereus_apply_vendor_patch("${_dc_build_source}"
        "libdatachannel-0004-retain-ice-lifetime-anchor.patch")

    _nereus_apply_vendor_patch("${_dc_build_source}"
        "libdatachannel-0005-defer-dtls-startup.patch")

    # These are function-scope normal variables. They configure only the
    # nested project and leave the parent cache, BUILD_SHARED_LIBS, and later
    # dependencies unchanged.
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_SHARED_DEPS_LIBS OFF)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(NO_WEBSOCKET ON)
    set(NO_MEDIA OFF)
    set(NO_EXAMPLES ON)
    set(NO_TESTS ON)
    set(PREFER_SYSTEM_LIB OFF)
    set(USE_SYSTEM_SRTP OFF)
    set(USE_SYSTEM_JUICE OFF)
    set(USE_SYSTEM_USRSCTP OFF)
    set(USE_SYSTEM_PLOG OFF)
    set(USE_SYSTEM_JSON OFF)
    set(USE_GNUTLS OFF)
    set(USE_MBEDTLS OFF)
    set(USE_NICE OFF)
    set(PLOG_INSTALL OFF)
    set(JSON_Install OFF)
    set(LIBSRTP_TEST_APPS OFF)
    set(ENABLE_WARNINGS_AS_ERRORS OFF)
    set(BUILD_WITH_WARNINGS OFF)
    set(sctp_build_programs OFF)
    set(sctp_werror OFF)
    set(NO_SERVER ON)

    add_subdirectory(
        "${_dc_build_source}"
        "${CMAKE_BINARY_DIR}/_deps/nereus_libdatachannel-build"
        EXCLUDE_FROM_ALL)
    # PR #327 (Windows x64, MinGW): usrsctp's user_environment.h defines its
    # min() and max() macros only when neither _MSC_VER nor __MINGW32__ is
    # set. MSVC's C <stdlib.h> supplies them; under MinGW they come only from
    # windows.h (minwindef.h), which the top-level CMakeLists' global
    # NOMINMAX turns off. libdatachannel itself defines NOMINMAX for MSVC
    # only. So usrsctp compiled min() and max() as implicit function
    # declarations and libNereusCore.dll failed to link ("undefined
    # reference to `min'", user_socket.c, sctp_output.c, sctputil.c and
    # more). Undefining NOMINMAX for the usrsctp target alone gives MinGW
    # back windows.h's macros, as portaudio_static and wdsp_static do for
    # the same reason; the fetched source is unchanged. MSVC is left as it
    # is. A pinned libdatachannel that no longer builds a usrsctp target
    # stops the configure on Windows rather than build without this.
    if(WIN32)
        if(NOT TARGET usrsctp)
            message(FATAL_ERROR
                "libdatachannel no longer builds a usrsctp target; the MinGW "
                "NOMINMAX exception in cmake/NereusRemoteMedia.cmake needs its "
                "new name.")
        endif()
        target_compile_options(usrsctp PRIVATE
            $<$<C_COMPILER_ID:GNU,Clang>:-UNOMINMAX>)
    endif()

    nereus_patch_libdatachannel_remote_description_first("${_dc_build_source}")
    nereus_patch_libdatachannel_dtls_mtu_first("${_dc_build_source}")

    # Older nested projects can still materialize these implementation
    # options in the parent cache. They are not NereusSDR user options.
    unset(PLOG_INSTALL CACHE)
    unset(JSON_Install CACHE)
endfunction()
