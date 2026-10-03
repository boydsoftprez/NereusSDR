# =================================================================
# cmake/FetchDependencyArchives.cmake  (NereusSDR)
# =================================================================
#
# no-port-check: NereusSDR-original.
#
# Fill a directory with every dependency a build downloads, so that a
# configure and build can take them from it instead of the network:
#
#   cmake -DNEREUS_DEPENDENCY_ARCHIVE_DIR=<dir>
#         [-DNEREUS_GIT_MIRROR_CONFIG=<file>]
#         [-DNEREUS_FETCH_PLATFORM=Linux|macOS|Windows]
#         -P cmake/FetchDependencyArchives.cmake
#
# The pins come from cmake/NereusDependencyArchives.cmake.
#
#   Archives  <dir>/<file>. A file already there is kept when its SHA-256
#             matches the pin and replaced when it does not. A download is
#             tried NEREUS_FETCH_ATTEMPTS times (default 5) with the waits
#             in NEREUS_FETCH_RETRY_DELAYS between them (default 10, 30,
#             60 and 120 seconds), and is kept only when its SHA-256
#             matches; a transient HTTP 5xx or a reset connection is
#             retried rather than failing the job.
#   Git       <dir>/git/<name>.git, a mirror clone. A mirror already there
#             is kept when the pinned ref resolves to the pinned commit,
#             fetched again when it does not, and cloned afresh as a last
#             resort; clone and fetch are retried like a download. With
#             NEREUS_GIT_MIRROR_CONFIG set, a git config file is written
#             there whose url.<mirror>.insteadOf entries send each declared
#             GIT_REPOSITORY to its verified mirror. CI adds that file to
#             git's global config with include.path; nothing here changes
#             any git configuration by itself.
#
# Anything in <dir> the manifest no longer names is removed, so a cache
# restored from an older run does not grow without bound. The script stops
# with an error naming the dependency when one cannot be fetched and
# verified; nothing unverified is ever left under its final name.
#
# NEREUS_FETCH_PLATFORM (default: this host) limits the work to the
# dependencies the manifest marks for that platform.
# NEREUS_FETCH_GIT_TIMEOUT (default 900) is the seconds one git clone or
# fetch attempt may take before it is stopped and counted as failed.
# NEREUS_DEPENDENCY_MANIFEST (default: the manifest next to this script)
# is there for the script's own tests.
#
# Modification history (NereusSDR):
#   2026-10-01: Created. A GitHub 504 on the libsodium and Opus archive
#               downloads failed two Linux CI jobs (run 36866003880).
#               J.J. Boyd (KG4VCF), with AI-assisted implementation via
#               Anthropic Claude Code.
# =================================================================

cmake_minimum_required(VERSION 3.20)

if(NOT NEREUS_DEPENDENCY_ARCHIVE_DIR)
    message(FATAL_ERROR "FetchDependencyArchives: set -DNEREUS_DEPENDENCY_ARCHIVE_DIR=<dir>")
endif()
file(TO_CMAKE_PATH "${NEREUS_DEPENDENCY_ARCHIVE_DIR}" _dir)
get_filename_component(_dir "${_dir}" ABSOLUTE)

if(NOT NEREUS_DEPENDENCY_MANIFEST)
    set(NEREUS_DEPENDENCY_MANIFEST "${CMAKE_CURRENT_LIST_DIR}/NereusDependencyArchives.cmake")
endif()
include("${NEREUS_DEPENDENCY_MANIFEST}")
get_filename_component(_source_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT NEREUS_FETCH_ATTEMPTS)
    set(NEREUS_FETCH_ATTEMPTS 5)
endif()
if(NOT DEFINED NEREUS_FETCH_RETRY_DELAYS)
    set(NEREUS_FETCH_RETRY_DELAYS 10 30 60 120)
endif()
# Seconds one git clone or fetch may take before it counts as a failed
# attempt (a hung transfer would otherwise hold the job to its timeout).
if(NOT NEREUS_FETCH_GIT_TIMEOUT)
    set(NEREUS_FETCH_GIT_TIMEOUT 900)
endif()

if(NOT NEREUS_FETCH_PLATFORM)
    if(CMAKE_HOST_WIN32)
        set(NEREUS_FETCH_PLATFORM Windows)
    elseif(CMAKE_HOST_APPLE)
        set(NEREUS_FETCH_PLATFORM macOS)
    else()
        set(NEREUS_FETCH_PLATFORM Linux)
    endif()
endif()

# Wait before attempt <attempt> (2, 3, ...).
function(_nereus_fetch_wait attempt)
    math(EXPR _index "${attempt} - 2")
    list(LENGTH NEREUS_FETCH_RETRY_DELAYS _count)
    if(_count EQUAL 0)
        return()
    endif()
    if(_index GREATER_EQUAL _count)
        math(EXPR _index "${_count} - 1")
    endif()
    list(GET NEREUS_FETCH_RETRY_DELAYS ${_index} _seconds)
    message(STATUS "  waiting ${_seconds} s before attempt ${attempt}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep "${_seconds}")
endfunction()

# ── Archives ─────────────────────────────────────────────────────────────
function(_nereus_fetch_archive name)
    set(_file "${NEREUS_DEPENDENCY_ARCHIVE_${name}_FILE}")
    set(_url "${NEREUS_DEPENDENCY_ARCHIVE_${name}_URL}")
    string(TOLOWER "${NEREUS_DEPENDENCY_ARCHIVE_${name}_SHA256}" _sha)
    set(_dest "${_dir}/${_file}")
    set(_part "${_dest}.part")

    if(EXISTS "${_dest}")
        file(SHA256 "${_dest}" _have)
        if(_have STREQUAL _sha)
            message(STATUS "${name}: ${_file} present, SHA-256 matches")
            return()
        endif()
        message(STATUS "${name}: ${_file} present with SHA-256 ${_have}, not the pinned ${_sha}; fetching again")
        file(REMOVE "${_dest}")
    endif()

    set(_failures "")
    foreach(_attempt RANGE 1 ${NEREUS_FETCH_ATTEMPTS})
        if(_attempt GREATER 1)
            _nereus_fetch_wait(${_attempt})
        endif()
        file(REMOVE "${_part}")
        message(STATUS "${name}: downloading ${_url} (attempt ${_attempt} of ${NEREUS_FETCH_ATTEMPTS})")
        file(DOWNLOAD "${_url}" "${_part}"
             STATUS _status
             LOG _log
             TLS_VERIFY ON
             INACTIVITY_TIMEOUT 120)
        list(GET _status 0 _code)
        list(GET _status 1 _message)
        # curl reports every HTTP error as code 22; the log has the status.
        if(_log MATCHES "returned error: ([0-9][0-9][0-9])")
            string(APPEND _message ", HTTP ${CMAKE_MATCH_1}")
        endif()
        if(_code EQUAL 0)
            file(SHA256 "${_part}" _got)
            if(_got STREQUAL _sha)
                file(RENAME "${_part}" "${_dest}")
                message(STATUS "${name}: ${_file} fetched, SHA-256 matches")
                return()
            endif()
            set(_reason "downloaded, but SHA-256 ${_got} is not the pinned ${_sha}")
        else()
            set(_reason "${_message} (code ${_code})")
        endif()
        message(STATUS "${name}: attempt ${_attempt} failed: ${_reason}")
        string(APPEND _failures "\n  attempt ${_attempt}: ${_reason}")
    endforeach()
    file(REMOVE "${_part}")
    message(FATAL_ERROR "${name}: could not fetch ${_url} with SHA-256 ${_sha}:${_failures}")
endfunction()

# ── Git repositories ─────────────────────────────────────────────────────
# Retry a git command; <out_ok> is TRUE when it eventually succeeds.
# <clean> (may be "") is removed before every attempt, so a clone that died
# halfway does not block the next one.
function(_nereus_git_retry out_ok label clean)
    foreach(_attempt RANGE 1 ${NEREUS_FETCH_ATTEMPTS})
        if(_attempt GREATER 1)
            _nereus_fetch_wait(${_attempt})
        endif()
        if(clean)
            file(REMOVE_RECURSE "${clean}")
        endif()
        message(STATUS "${label} (attempt ${_attempt} of ${NEREUS_FETCH_ATTEMPTS})")
        execute_process(COMMAND ${ARGN}
                        TIMEOUT ${NEREUS_FETCH_GIT_TIMEOUT}
                        RESULT_VARIABLE _result
                        OUTPUT_VARIABLE _out
                        ERROR_VARIABLE _err)
        if(_result EQUAL 0)
            set(${out_ok} TRUE PARENT_SCOPE)
            return()
        endif()
        string(STRIP "${_err}" _err)
        message(STATUS "  failed (${_result}): ${_err}")
    endforeach()
    set(${out_ok} FALSE PARENT_SCOPE)
endfunction()

# <out_ok> is TRUE when <mirror> resolves <ref> to <commit>.
function(_nereus_git_mirror_ok out_ok mirror ref commit)
    set(${out_ok} FALSE PARENT_SCOPE)
    if(NOT EXISTS "${mirror}/HEAD")
        return()
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" --git-dir "${mirror}" rev-parse --verify --quiet "${ref}^{commit}"
                    TIMEOUT 120
                    RESULT_VARIABLE _result
                    OUTPUT_VARIABLE _resolved
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_result EQUAL 0 AND _resolved STREQUAL commit)
        set(${out_ok} TRUE PARENT_SCOPE)
    endif()
endfunction()

function(_nereus_fetch_git name)
    set(_url "${NEREUS_DEPENDENCY_GIT_${name}_URL}")
    set(_ref "${NEREUS_DEPENDENCY_GIT_${name}_REF}")
    string(TOLOWER "${NEREUS_DEPENDENCY_GIT_${name}_COMMIT}" _commit)
    set(_declared_in "${NEREUS_DEPENDENCY_GIT_${name}_DECLARED_IN}")
    set(_mirror "${_dir}/git/${name}.git")

    # The declaration must still name this repository and ref; a pin moved
    # there and not here would otherwise go on being checked against the
    # old one.
    if(_declared_in)
        # Only a GIT_REPOSITORY line naming this URL and the GIT_TAG line
        # that follows it count, so a comment that still names an old pin,
        # or another declaration's tag, does not satisfy the check.
        file(STRINGS "${_source_root}/${_declared_in}" _lines)
        set(_url_ok FALSE)
        set(_ref_ok FALSE)
        set(_in_block FALSE)
        foreach(_line IN LISTS _lines)
            if(_line MATCHES "^[ \t]*GIT_REPOSITORY[ \t]+([^ \t#]+)")
                set(_in_block FALSE)
                if(CMAKE_MATCH_1 STREQUAL _url)
                    set(_url_ok TRUE)
                    set(_in_block TRUE)
                endif()
            elseif(_in_block AND _line MATCHES "^[ \t]*GIT_TAG[ \t]+([^ \t#]+)")
                set(_in_block FALSE)
                if(CMAKE_MATCH_1 STREQUAL _ref)
                    set(_ref_ok TRUE)
                endif()
            endif()
        endforeach()
        if(NOT _url_ok OR NOT _ref_ok)
            message(FATAL_ERROR
                "${name}: ${_declared_in} no longer declares ${_url} at ${_ref}. "
                "Update cmake/NereusDependencyArchives.cmake to the new pin.")
        endif()
    endif()

    _nereus_git_mirror_ok(_ok "${_mirror}" "${_ref}" "${_commit}")
    if(_ok)
        message(STATUS "${name}: mirror present, ${_ref} is ${_commit}")
        return()
    endif()

    if(EXISTS "${_mirror}/HEAD")
        _nereus_git_retry(_fetched "${name}: updating mirror from ${_url}" ""
            "${GIT_EXECUTABLE}" --git-dir "${_mirror}" fetch --prune --tags origin)
        _nereus_git_mirror_ok(_ok "${_mirror}" "${_ref}" "${_commit}")
        if(_ok)
            message(STATUS "${name}: mirror updated, ${_ref} is ${_commit}")
            return()
        endif()
    endif()

    file(REMOVE_RECURSE "${_mirror}")
    file(MAKE_DIRECTORY "${_dir}/git")
    _nereus_git_retry(_cloned "${name}: cloning ${_url}" "${_mirror}.part"
        "${CMAKE_COMMAND}" -E chdir "${_dir}/git"
        "${GIT_EXECUTABLE}" clone --mirror --quiet "${_url}" "${name}.git.part")
    if(NOT _cloned)
        file(REMOVE_RECURSE "${_mirror}.part")
        message(FATAL_ERROR "${name}: could not clone ${_url}")
    endif()
    _nereus_git_mirror_ok(_ok "${_mirror}.part" "${_ref}" "${_commit}")
    if(NOT _ok)
        file(REMOVE_RECURSE "${_mirror}.part")
        message(FATAL_ERROR "${name}: ${_url} does not resolve ${_ref} to the pinned commit ${_commit}")
    endif()
    file(RENAME "${_mirror}.part" "${_mirror}")
    message(STATUS "${name}: mirror cloned, ${_ref} is ${_commit}")
endfunction()

# ── Run ──────────────────────────────────────────────────────────────────
file(MAKE_DIRECTORY "${_dir}")
message(STATUS "Dependency downloads for ${NEREUS_FETCH_PLATFORM} into ${_dir}")

set(_keep_files "")
foreach(_name IN LISTS NEREUS_DEPENDENCY_ARCHIVES)
    if(NEREUS_FETCH_PLATFORM IN_LIST NEREUS_DEPENDENCY_ARCHIVE_${_name}_PLATFORMS)
        _nereus_fetch_archive(${_name})
        list(APPEND _keep_files "${NEREUS_DEPENDENCY_ARCHIVE_${_name}_FILE}")
    endif()
endforeach()

set(_git_repos "")
foreach(_name IN LISTS NEREUS_DEPENDENCY_GIT_REPOS)
    if(NEREUS_FETCH_PLATFORM IN_LIST NEREUS_DEPENDENCY_GIT_${_name}_PLATFORMS)
        list(APPEND _git_repos ${_name})
    endif()
endforeach()
if(_git_repos)
    find_program(GIT_EXECUTABLE git REQUIRED)
    foreach(_name IN LISTS _git_repos)
        _nereus_fetch_git(${_name})
    endforeach()
endif()

# Prune what the manifest no longer names for this platform.
file(GLOB _present LIST_DIRECTORIES false RELATIVE "${_dir}" "${_dir}/*")
foreach(_entry IN LISTS _present)
    if(NOT _entry IN_LIST _keep_files)
        message(STATUS "removing ${_entry}: not in the manifest")
        file(REMOVE "${_dir}/${_entry}")
    endif()
endforeach()
if(IS_DIRECTORY "${_dir}/git")
    file(GLOB _present LIST_DIRECTORIES true RELATIVE "${_dir}/git" "${_dir}/git/*")
    foreach(_entry IN LISTS _present)
        string(REGEX REPLACE "\\.git$" "" _repo "${_entry}")
        if(NOT _entry MATCHES "\\.git$" OR NOT _repo IN_LIST _git_repos)
            message(STATUS "removing git/${_entry}: not in the manifest")
            file(REMOVE_RECURSE "${_dir}/git/${_entry}")
        endif()
    endforeach()
endif()

# The insteadOf entries, written to a separate file (never to any git
# configuration directly).
if(NEREUS_GIT_MIRROR_CONFIG)
    file(TO_CMAKE_PATH "${NEREUS_GIT_MIRROR_CONFIG}" _config)
    file(REMOVE "${_config}")
    file(WRITE "${_config}" "")
    foreach(_name IN LISTS _git_repos)
        set(_mirror "${_dir}/git/${_name}.git")
        if(_mirror MATCHES "^/")
            set(_mirror_url "file://${_mirror}")
        else()
            set(_mirror_url "file:///${_mirror}")
        endif()
        execute_process(COMMAND "${GIT_EXECUTABLE}" config --file "${_config}"
                                "url.${_mirror_url}.insteadOf" "${NEREUS_DEPENDENCY_GIT_${_name}_URL}"
                        RESULT_VARIABLE _result)
        if(NOT _result EQUAL 0)
            message(FATAL_ERROR "${_name}: could not write ${_config}")
        endif()
    endforeach()
    message(STATUS "git mirror redirects written to ${_config}")
endif()
