# r8brain-free-src vendored at SHA 5c44bebe9c477d47b1dc7037fcaae2794ff2b4e1
# Upstream: https://github.com/avaneev/r8brain-free-src
# License: MIT (see LICENSE.txt in this directory)
#
# Header-only library; r8bbase.cpp is a NereusSDR-added compilation
# anchor (one #include "CDSPResampler.h") so the function-local statics
# in CDSPRealFFT.h get instantiated exactly once.

add_library(r8brain OBJECT r8bbase.cpp)
target_include_directories(r8brain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(r8brain PROPERTIES POSITION_INDEPENDENT_CODE ON)

# r8brain uses long-standing pragma-disable idioms internally; suppress
# the typical warnings we'd get on Clang/GCC so the vendored headers
# don't pollute the NereusSDR build log.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(r8brain PRIVATE
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-sign-compare
        -Wno-shadow
    )
endif()
