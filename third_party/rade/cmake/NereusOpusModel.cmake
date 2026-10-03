# =================================================================
# third_party/rade/cmake/NereusOpusModel.cmake  (NereusSDR)
# =================================================================
#
# no-port-check: NereusSDR-original. Part of BuildOpus.cmake's vendored
# patch (5).
#
# Run by the Opus ExternalProject steps with cmake -P. Keeps the Opus
# DRED/OSCE model (opus_data-<SHA256>.tar.gz, 183 MB) out of the network
# path:
#
#   MODE=stash  copy the model from SOURCE_DIR to STASH, when SOURCE_DIR has
#               one whose SHA-256 is SHA256 and STASH does not already hold
#               it. STASH lies outside the Opus source tree, so it survives
#               the re-extract that wipes the tree (an Opus URL or hash
#               change). Runs before the download step (to keep the copy a
#               build dir already has) and after the configure step (to keep
#               the copy autogen.sh / download_model.bat just fetched).
#   MODE=seed   copy into SOURCE_DIR the first of MODEL (the pre-fetched
#               copy, may be empty) and STASH that exists, after checking its
#               SHA-256 is SHA256; a mismatch stops the build. With neither,
#               nothing is copied and the pinned Opus downloads the model as
#               upstream does. dnn/download_model.sh and download_model.bat
#               skip the download when the file is already in the tree.
#
# Nothing is copied without its SHA-256 matching the pin first.
#
# Modification history (NereusSDR):
#   2026-10-01: Created. J.J. Boyd (KG4VCF), with AI-assisted
#               implementation via Anthropic Claude Code.
# =================================================================

cmake_minimum_required(VERSION 3.21)

foreach(_required MODE STASH SOURCE_DIR SHA256)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "NereusOpusModel: ${_required} is required")
    endif()
endforeach()
string(TOLOWER "${SHA256}" _pin)
set(_name "opus_data-${_pin}.tar.gz")
set(_in_tree "${SOURCE_DIR}/${_name}")

# TRUE in <out> when <file> exists and its SHA-256 is the pin.
function(_nereus_model_ok out file)
    set(${out} FALSE PARENT_SCOPE)
    if(EXISTS "${file}" AND NOT IS_DIRECTORY "${file}")
        file(SHA256 "${file}" _have)
        if(_have STREQUAL _pin)
            set(${out} TRUE PARENT_SCOPE)
        endif()
    endif()
endfunction()

if(MODE STREQUAL "stash")
    _nereus_model_ok(_stash_ok "${STASH}")
    if(_stash_ok)
        return()
    endif()
    _nereus_model_ok(_tree_ok "${_in_tree}")
    if(_tree_ok)
        get_filename_component(_stash_dir "${STASH}" DIRECTORY)
        file(MAKE_DIRECTORY "${_stash_dir}")
        # The universal macOS build runs two Opus projects at once with one
        # STASH; each writes its own temporary name and the rename is atomic.
        string(MD5 _tag "${SOURCE_DIR}")
        file(COPY_FILE "${_in_tree}" "${STASH}.${_tag}.part")
        file(RENAME "${STASH}.${_tag}.part" "${STASH}")
        message(STATUS "Opus model: kept a verified copy at ${STASH}")
    endif()
elseif(MODE STREQUAL "seed")
    set(_from "")
    if(DEFINED MODEL AND NOT "${MODEL}" STREQUAL "" AND EXISTS "${MODEL}")
        set(_from "${MODEL}")
    elseif(EXISTS "${STASH}")
        set(_from "${STASH}")
    endif()
    if(_from STREQUAL "")
        # An Opus archive that carries the model (the offline kit's
        # with-model zip) already put it in the tree.
        _nereus_model_ok(_tree_ok "${_in_tree}")
        if(_tree_ok)
            message(STATUS "Opus model: the Opus archive carries it (SHA-256 matches)")
            return()
        endif()
        message(STATUS "Opus model: no local copy, the Opus build downloads it")
        return()
    endif()
    _nereus_model_ok(_ok "${_from}")
    if(NOT _ok)
        file(SHA256 "${_from}" _have)
        message(FATAL_ERROR "Opus model: ${_from} has SHA-256 ${_have}, not the pinned ${_pin}")
    endif()
    file(COPY_FILE "${_from}" "${_in_tree}" ONLY_IF_DIFFERENT)
    message(STATUS "Opus model: using ${_from} (SHA-256 matches)")
else()
    message(FATAL_ERROR "NereusOpusModel: MODE must be stash or seed, not '${MODE}'")
endif()
