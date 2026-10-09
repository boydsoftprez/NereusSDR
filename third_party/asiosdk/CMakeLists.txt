# Steinberg ASIO SDK 2.3.4, vendored unchanged (see VERSION.txt).
# License: dual, the Steinberg ASIO License or GPL version 3; NereusSDR
# takes it under GPL-3.0 (LICENSE.txt here, LICENSE-NOTICE at the root).
# The host helper files carry their own embedded BSD-style notice.
#
# asiosdk_host is the host side of the SDK: the ASIO entry points
# (common/asio.cpp), the driver loader (host/asiodrivers.cpp) and the
# Windows driver list read from the registry (host/pc/asiolist.cpp). Only
# the mic helper nereus-audio-capture links it. ASIO exists only on
# Windows, so the root CMakeLists.txt adds this directory inside
# if(WIN32); the guard below keeps it inert if it is ever added elsewhere.

if(NOT WIN32)
    return()
endif()

add_library(asiosdk_host STATIC
    common/asio.cpp
    host/asiodrivers.cpp
    host/pc/asiolist.cpp
)

target_include_directories(asiosdk_host PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/host
    ${CMAKE_CURRENT_SOURCE_DIR}/host/pc
)

# asiolist.cpp reads the installed drivers from the registry (advapi32)
# and creates each driver as a COM object (ole32).
target_link_libraries(asiosdk_host PUBLIC ole32 advapi32)

# The SDK is compiled as released; keep its warnings out of the
# NereusSDR build log.
if(MSVC)
    target_compile_options(asiosdk_host PRIVATE /w)
else()
    target_compile_options(asiosdk_host PRIVATE -w)
endif()
