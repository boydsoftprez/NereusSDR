# no-port-check: NereusSDR-original.
# R-R3-49: a project that compiles copies of libdatachannel's
# src/peerconnection.cpp and src/impl/dtlstransport.cpp as libdatachannel's
# own CMake names it (datachannel and datachannel-static) and asks
# nereus_patch_libdatachannel_remote_description_first() and
# nereus_patch_libdatachannel_dtls_mtu_first() to swap in the patched
# copies. Configured by tests/scripts/check-libdatachannel-patch.cmake;
# never built.
cmake_minimum_required(VERSION 3.20)
project(nereus_libdatachannel_patch NONE)
include("${REPO}/cmake/NereusRemoteMedia.cmake")
add_subdirectory("${DC_DIR}" datachannel)
nereus_patch_libdatachannel_remote_description_first("${DC_DIR}")
nereus_patch_libdatachannel_dtls_mtu_first("${DC_DIR}")
foreach(_target IN ITEMS datachannel datachannel-static)
    get_target_property(_sources ${_target} SOURCES)
    message(STATUS "nereus-dc-patch ${_target}: ${_sources}")
endforeach()
