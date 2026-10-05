# no-port-check: NereusSDR-original, test-only vendor copy/observers.
# Modification history: 2026-10-05, J.J. Boyd (KG4VCF), OpenAI Codex.
find_package(OpenSSL REQUIRED)
set(_dtls_hooks_dir "${CMAKE_CURRENT_LIST_DIR}")
function(_nereus_dtls_replace path anchor replacement)
    file(READ "${path}" _text)
    string(FIND "${_text}" "${anchor}" _first)
    string(FIND "${_text}" "${anchor}" _last REVERSE)
    if(_first EQUAL -1 OR NOT _first EQUAL _last)
        message(FATAL_ERROR "DTLS fixture anchor absent or ambiguous in ${path}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _text "${_text}")
    file(WRITE "${path}" "${_text}")
endfunction()
set(_fixture "${CMAKE_CURRENT_BINARY_DIR}/dtls-startup-fixture")
file(MAKE_DIRECTORY "${_fixture}")
file(COPY "${libdatachannel_SOURCE_DIR}/src" "${libdatachannel_SOURCE_DIR}/include"
     DESTINATION "${_fixture}")
# The production MTU overlay is a separately generated source leaf.
file(COPY "${CMAKE_BINARY_DIR}/_deps/nereus-patched/libdatachannel/src/impl/dtlstransport.cpp"
     DESTINATION "${_fixture}/src/impl")
file(COPY "${CMAKE_BINARY_DIR}/_deps/nereus-patched/libdatachannel/src/peerconnection.cpp"
     DESTINATION "${_fixture}/src")
file(WRITE "${_fixture}/provenance.txt" "")
foreach(_leaf IN ITEMS src/impl/peerconnection.cpp src/impl/dtlstransport.cpp)
    file(SHA256 "${_fixture}/${_leaf}" _original)
    file(APPEND "${_fixture}/provenance.txt" "input ${_leaf} ${_original}\n")
endforeach()
foreach(_leaf IN ITEMS peerconnection icetransport dtlstransport)
    if(_leaf STREQUAL "dtlstransport")
        set(_access "protected:")
    else()
        set(_access "private:")
    endif()
    _nereus_dtls_replace("${_fixture}/src/impl/${_leaf}.hpp" "\n${_access}\n"
        "\nfriend struct NereusDtlsStartupTestAccess;\n${_access}\n")
endforeach()
_nereus_dtls_replace("${_fixture}/src/impl/dtlstransport.cpp"
    "void DtlsTransport::start() {\n/**"
    "void DtlsTransport::start() {\n\tnereusDtlsStartupStart(this);\n/**")
_nereus_dtls_replace("${_fixture}/src/impl/peerconnection.cpp"
    "shared_ptr<DtlsTransport> PeerConnection::initDtlsTransport() {"
    "shared_ptr<DtlsTransport> PeerConnection::initDtlsTransport() {\n\tnereusDtlsStartupInit(this);")
foreach(_leaf IN ITEMS peerconnection dtlstransport)
    file(READ "${_fixture}/src/impl/${_leaf}.cpp" _text)
    string(FIND "${_text}" "*/\n" _end)
    if(_end EQUAL -1)
        message(FATAL_ERROR "Vendor fixture source has no own leading license notice")
    endif()
    math(EXPR _end "${_end} + 3")
    string(SUBSTRING "${_text}" 0 ${_end} _notice)
    string(SUBSTRING "${_text}" ${_end} -1 _body)
    file(WRITE "${_fixture}/src/impl/${_leaf}.cpp"
        "${_notice}\n#include \"${_dtls_hooks_dir}/Hooks.h\"\n${_body}")
endforeach()
get_target_property(_sources datachannel-static SOURCES)
set(_fixture_sources "")
foreach(_source IN LISTS _sources)
    string(REPLACE "${libdatachannel_SOURCE_DIR}/" "${_fixture}/" _source "${_source}")
    string(REPLACE "${CMAKE_BINARY_DIR}/_deps/nereus-patched/libdatachannel/" "${_fixture}/" _source "${_source}")
    list(APPEND _fixture_sources "${_source}")
endforeach()
add_library(nereus_dtls_startup_fixture STATIC EXCLUDE_FROM_ALL ${_fixture_sources}
    "${_dtls_hooks_dir}/Access.c")
set_target_properties(nereus_dtls_startup_fixture PROPERTIES AUTOMOC OFF CXX_STANDARD 17 C_STANDARD 11)
get_target_property(_definitions datachannel-static COMPILE_DEFINITIONS)
get_target_property(_links datachannel-static LINK_LIBRARIES)
get_target_property(_includes datachannel-static INCLUDE_DIRECTORIES)
target_compile_definitions(nereus_dtls_startup_fixture PUBLIC ${_definitions} JUICE_STATIC)
target_link_libraries(nereus_dtls_startup_fixture PRIVATE ${_links} PUBLIC OpenSSL::SSL)
target_include_directories(nereus_dtls_startup_fixture PUBLIC
    "${_fixture}/src" "${_fixture}/include" "${_fixture}/include/rtc"
    "${_dtls_hooks_dir}" ${_includes}
    "${libdatachannel_SOURCE_DIR}/deps/plog/include"
    "${libdatachannel_SOURCE_DIR}/deps/usrsctp/usrsctplib"
    "${libdatachannel_SOURCE_DIR}/deps/libsrtp/include"
    "${libdatachannel_SOURCE_DIR}/deps/libjuice/include"
    "${libdatachannel_SOURCE_DIR}/deps/libjuice/src"
    "${libdatachannel_SOURCE_DIR}/deps/libjuice/include/juice")
foreach(_leaf IN ITEMS src/impl/peerconnection.cpp src/impl/dtlstransport.cpp)
    file(SHA256 "${_fixture}/${_leaf}" _hash)
    file(APPEND "${_fixture}/provenance.txt" "overlay ${_leaf} ${_hash}\n")
endforeach()
