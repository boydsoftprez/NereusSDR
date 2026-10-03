# no-port-check: NereusSDR-original.
# The built vendor trees must contain exactly the reviewed patch series.
# Reverse the patches on scratch copies and compare every covered file with
# its pinned source; this also catches omissions in public/internal headers.
# Modification history (NereusSDR):
#   2026-09-27: original implementation by J.J. Boyd (KG4VCF), with
#               AI-assisted implementation via OpenAI Codex.
foreach(_required IN ITEMS REPO BUILD ORIGINAL_JUICE ORIGINAL_DC PATCHED_JUICE DATACHANNEL WORK)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "check-relay-cleanup-patches: -D${_required}= is required")
    endif()
endforeach()
# 2026-10-03: include the retained-registry poll-worker patch in exact
# materialization/reverse proof. J.J. Boyd (KG4VCF), OpenAI Codex.
find_program(_git git REQUIRED)
set(_git_in_scratch "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${WORK}"
    "LC_ALL=C" "${_git}")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/juice" "${WORK}/datachannel/src/impl"
    "${WORK}/datachannel/include/rtc")
file(COPY "${PATCHED_JUICE}/" DESTINATION "${WORK}/juice")
file(COPY "${DATACHANNEL}/src/impl/icetransport.cpp"
          "${DATACHANNEL}/src/impl/icetransport.hpp"
     DESTINATION "${WORK}/datachannel/src/impl")
file(COPY "${DATACHANNEL}/include/rtc/configuration.hpp"
     DESTINATION "${WORK}/datachannel/include/rtc")

# These hashes are the four materialized phone files from the approved
# 0864b04f libjuice patch plus this Core-only ICE anchor patch. A skipped
# or partly applied patch fails here before any reverse operation.
foreach(_entry IN ITEMS
        "include/juice/juice.h|bf00657bec018f8fa9740a41ba14565b74de9d5d8f44a4391fe52a19cb5745c1"
        "src/agent.c|82341e8ffb23606253f625abec515972c5cadfb6b15923d04ff45eda2ad0831c"
        "src/agent.h|fc1d88cf387189b645e31228b6e9f68a8bf1a4efad8f483a710fc81125ea9656"
        "src/juice.c|408a423f1c3f4a7c4a17ce7b4612ad1e5295b3c3c4b8fb7024a610afdd36b7b8"
        "src/conn_poll.c|5ccfb2f2622c43b5bb5b3ce727c9ce638b5bc4519b8b3c2ee0f927e1efbd3afb")
    string(REPLACE "|" ";" _entry "${_entry}")
    list(GET _entry 0 _relative)
    list(GET _entry 1 _expected)
    file(SHA256 "${PATCHED_JUICE}/${_relative}" _actual)
    if(NOT _actual STREQUAL _expected)
        message(FATAL_ERROR "built libjuice ${_relative} differs from approved materialization")
    endif()
endforeach()
foreach(_entry IN ITEMS
        "include/rtc/configuration.hpp|b36120b925508cb132f00b8856f1d79582887dc6061a44fbd05f32e9aa74ff46"
        "src/impl/icetransport.hpp|85029d443fca63b00032779cc5a758d445089723e05f2a0248396ab3c7662cc0"
        "src/impl/icetransport.cpp|1a92e9c375c08182c86d434f7a5301efdf20d7353a2687fb4a5e54409f4ff3bd")
    string(REPLACE "|" ";" _entry "${_entry}")
    list(GET _entry 0 _relative)
    list(GET _entry 1 _expected)
    file(SHA256 "${DATACHANNEL}/${_relative}" _actual)
    if(NOT _actual STREQUAL _expected)
        message(FATAL_ERROR "built libdatachannel ${_relative} differs from approved Core patch")
    endif()
endforeach()

foreach(_patch IN ITEMS
        libjuice-0003-keep-poll-worker-with-retained-agents.patch
        libjuice-0002-bounded-turn-release-lifecycle.patch
        libjuice-0001-give-turn-allocations-back.patch)
    execute_process(COMMAND ${_git_in_scratch} apply --reverse --verbose
            "${REPO}/cmake/patches/${_patch}"
        WORKING_DIRECTORY "${WORK}/juice" RESULT_VARIABLE _result
        OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
    string(FIND "${_output}${_error}" "Applied patch" _applied)
    if(NOT _result EQUAL 0 OR _applied EQUAL -1)
        message(FATAL_ERROR "${_patch} is absent or changed in the built libjuice: ${_output}${_error}")
    endif()
endforeach()

foreach(_relative IN ITEMS include/juice/juice.h src/agent.c src/agent.h src/juice.c src/conn_poll.c)
    file(SHA256 "${WORK}/juice/${_relative}" _restored)
    file(SHA256 "${ORIGINAL_JUICE}/${_relative}" _pinned)
    if(NOT _restored STREQUAL _pinned)
        message(FATAL_ERROR "${_relative} differs from the pinned libjuice after reversing patches")
    endif()
endforeach()

execute_process(COMMAND ${_git_in_scratch} apply --reverse --verbose
        "${REPO}/cmake/patches/libdatachannel-0004-retain-ice-lifetime-anchor.patch"
    WORKING_DIRECTORY "${WORK}/datachannel" RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
string(FIND "${_output}${_error}" "Applied patch" _applied)
if(NOT _result EQUAL 0 OR _applied EQUAL -1)
    message(FATAL_ERROR "ICE lifetime anchor patch is absent or changed: ${_output}${_error}")
endif()
file(SHA256 "${WORK}/datachannel/src/impl/icetransport.cpp" _phone_ice)
if(NOT _phone_ice STREQUAL "1353c0be330058d05d4e4a562f83c90313d58acef08364604ae58e3ddca49e2f")
    message(FATAL_ERROR "reversing the Core anchor did not recover the reviewed phone ICE source")
endif()
foreach(_relative IN ITEMS include/rtc/configuration.hpp src/impl/icetransport.hpp)
    file(SHA256 "${WORK}/datachannel/${_relative}" _restored)
    file(SHA256 "${ORIGINAL_DC}/${_relative}" _pinned)
    if(NOT _restored STREQUAL _pinned)
        message(FATAL_ERROR "${_relative} differs from its pin after reversing the Core anchor")
    endif()
endforeach()
execute_process(COMMAND ${_git_in_scratch} apply --reverse --verbose
        "${REPO}/cmake/patches/libdatachannel-0003-retain-juice-agent-through-turn-release.patch"
    WORKING_DIRECTORY "${WORK}/datachannel" RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
string(FIND "${_output}${_error}" "Applied patch" _applied)
if(NOT _result EQUAL 0 OR _applied EQUAL -1)
    message(FATAL_ERROR "ICE retirement patch is absent or changed: ${_output}${_error}")
endif()
file(SHA256 "${WORK}/datachannel/src/impl/icetransport.cpp" _restored)
file(SHA256 "${ORIGINAL_DC}/src/impl/icetransport.cpp" _pinned)
if(NOT _restored STREQUAL _pinned)
    message(FATAL_ERROR "icetransport.cpp differs from the pin after reversing both patches")
endif()

if(EXISTS "${BUILD}/build.ninja")
    file(STRINGS "${BUILD}/build.ninja" _juice_rules
         REGEX "^build .*juice-static.dir/src/agent.c.o: ")
    file(STRINGS "${BUILD}/build.ninja" _ice_rules
         REGEX "^build .*datachannel-static.dir/src/impl/icetransport.cpp.o: ")
    string(FIND "${_juice_rules}" "${PATCHED_JUICE}/src/agent.c" _juice_at)
    string(FIND "${_ice_rules}" "${DATACHANNEL}/src/impl/icetransport.cpp" _ice_at)
    if(_juice_at EQUAL -1 OR _ice_at EQUAL -1)
        message(FATAL_ERROR "Ninja does not compile the patched libjuice and ICE sources")
    endif()
endif()
message(STATUS "PASS: compiled vendor sources match the reviewed relay cleanup patches")
