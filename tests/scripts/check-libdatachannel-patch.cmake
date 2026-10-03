# no-port-check: NereusSDR-original.
# =================================================================
# tests/scripts/check-libdatachannel-patch.cmake  (NereusSDR)
# =================================================================
# R-R3-49. nereus_patch_libdatachannel_remote_description_first() and
# nereus_patch_libdatachannel_dtls_mtu_first() (cmake/NereusRemoteMedia.cmake)
# compile libdatachannel from copies of src/peerconnection.cpp and
# src/impl/dtlstransport.cpp with the changes in
# cmake/patches/libdatachannel-keep-remote-description-first.cpp and
# cmake/patches/libdatachannel-set-dtls-mtu-before-incoming.cpp. This fails
# when either change stops applying to the pinned source:
#
#   cmake -DREPO=<source dir> -DPC_CPP=<libdatachannel src/peerconnection.cpp>
#         -DDTLS_CPP=<libdatachannel src/impl/dtlstransport.cpp>
#         -DWORK=<scratch dir> -P check-libdatachannel-patch.cmake
#
# It configures tests/cmake/libdatachannel-patch over copies of the pinned
# files and checks that both library targets compile both patched copies,
# that the first keeps the remote description before the ICE agent takes it
# and the second sets the DTLS MTU before it takes incoming records, that
# neither has the old order, and that each change carries its pinned file's
# own licence notice byte for byte. Configures only; nothing is built or
# downloaded.
# =================================================================
# Modification history (NereusSDR):
#   2026-09-27: original implementation for NereusSDR by J.J. Boyd
#               (KG4VCF), with AI-assisted implementation via Anthropic
#               Claude Code.
#   2026-09-27: the DTLS MTU change too (DTLS_CPP). J.J. Boyd (KG4VCF),
#               with AI-assisted implementation via Anthropic Claude Code.
# =================================================================
foreach(_required IN ITEMS REPO PC_CPP DTLS_CPP WORK)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "check-libdatachannel-patch: -D${_required}= is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/datachannel/src/impl")
file(COPY "${PC_CPP}" DESTINATION "${WORK}/datachannel/src")
file(COPY "${DTLS_CPP}" DESTINATION "${WORK}/datachannel/src/impl")
file(COPY "${REPO}/tests/cmake/libdatachannel-patch/datachannel/CMakeLists.txt"
     DESTINATION "${WORK}/datachannel")

set(_build "${WORK}/build")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${REPO}/tests/cmake/libdatachannel-patch"
            -B "${_build}" "-DREPO=${REPO}" "-DDC_DIR=${WORK}/datachannel"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "the libdatachannel change did not apply:\n${_out}\n${_err}")
endif()

set(_patched "${_build}/_deps/nereus-patched/libdatachannel/src/peerconnection.cpp")
set(_patched_dtls "${_build}/_deps/nereus-patched/libdatachannel/src/impl/dtlstransport.cpp")
foreach(_copy IN ITEMS "${_patched}" "${_patched_dtls}")
    if(NOT EXISTS "${_copy}")
        message(FATAL_ERROR "no patched copy at ${_copy}")
    endif()
endforeach()
foreach(_target IN ITEMS datachannel datachannel-static)
    string(FIND "${_out}" "nereus-dc-patch ${_target}: ${_patched};${_patched_dtls}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "${_target} does not compile both patched copies:\n${_out}")
    endif()
endforeach()

file(READ "${_patched}" _copy)
set(_kept "\timpl()->processRemoteDescription(description);\n")
set(_taken "\ticeTransport->setRemoteDescription(description); // ICE transport might reject the description\n")
set(_old "\timpl()->processRemoteDescription(std::move(description));\n")
string(FIND "${_copy}" "${_kept}" _kept_at)
string(FIND "${_copy}" "${_taken}" _taken_at)
string(FIND "${_copy}" "${_old}" _old_at)
if(_kept_at EQUAL -1 OR _taken_at EQUAL -1 OR NOT _kept_at LESS _taken_at)
    message(FATAL_ERROR "the patched copy does not keep the remote description before the "
                        "ICE agent takes it")
endif()
if(NOT _old_at EQUAL -1)
    message(FATAL_ERROR "the patched copy still has the old order")
endif()

# The DTLS change: in the OpenSSL start(), the MTU is set before the
# incoming callback is registered, and the old order is gone.
file(READ "${_patched_dtls}" _dtls)
string(FIND "${_dtls}" "void DtlsTransport::start() {\n\tPLOG_DEBUG << \"Starting DTLS transport\";\n\tregisterIncoming();\n\tchangeState(State::Connecting);\n\n\tint ret, err;\n\t{\n\t\tstd::lock_guard lock(mSslMutex);\n\n\t\tsize_t mtu" _dtls_old)
if(NOT _dtls_old EQUAL -1)
    message(FATAL_ERROR "the patched DTLS copy still takes incoming records before the MTU")
endif()
string(FIND "${_dtls}" "\t\tSSL_set_mtu(mSsl, static_cast<unsigned int>(mtu));\n\t\tPLOG_VERBOSE << \"DTLS MTU set to \" << mtu << \" before incoming records are taken\";\n\t}\n\n\tregisterIncoming();\n\tchangeState(State::Connecting);\n" _dtls_new)
if(_dtls_new EQUAL -1)
    message(FATAL_ERROR "the patched DTLS copy does not set the MTU before it takes "
                        "incoming records")
endif()

# Each change's notice: its pinned file's own, byte for byte, up to the end
# of its first comment.
foreach(_pair IN ITEMS "PC_CPP|libdatachannel-keep-remote-description-first.cpp"
                       "DTLS_CPP|libdatachannel-set-dtls-mtu-before-incoming.cpp")
    string(REPLACE "|" ";" _pair "${_pair}")
    list(GET _pair 0 _var)
    list(GET _pair 1 _change_file)
    file(READ "${${_var}}" _pinned)
    string(FIND "${_pinned}" "*/\n" _end)
    if(_end EQUAL -1)
        message(FATAL_ERROR "the pinned ${${_var}} has no leading notice")
    endif()
    math(EXPR _length "${_end} + 3")
    string(SUBSTRING "${_pinned}" 0 ${_length} _notice)
    file(READ "${REPO}/cmake/patches/${_change_file}" _change)
    string(FIND "${_change}" "${_notice}" _notice_at)
    if(NOT _notice_at EQUAL 0)
        message(FATAL_ERROR "${_change_file} does not open with the pinned file's own notice")
    endif()
endforeach()
message(STATUS "PASS: the libdatachannel changes apply to ${PC_CPP} and ${DTLS_CPP}")
